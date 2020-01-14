// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include <stdint.h>

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Sends a datagram on write().
class QuicTransport::DatagramUnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit DatagramUnderlyingSink(QuicTransport* quic_transport)
      : quic_transport_(quic_transport) {}

  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController*,
                      ExceptionState&) override {
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override {
    auto v8chunk = chunk.V8Value();
    if (v8chunk->IsArrayBuffer()) {
      DOMArrayBuffer* data =
          V8ArrayBuffer::ToImpl(v8chunk.As<v8::ArrayBuffer>());
      return SendDatagram({static_cast<const uint8_t*>(data->Data()),
                           data->ByteLengthAsSizeT()});
    }

    auto* isolate = script_state->GetIsolate();
    if (v8chunk->IsArrayBufferView()) {
      NotShared<DOMArrayBufferView> data =
          ToNotShared<NotShared<DOMArrayBufferView>>(isolate, v8chunk,
                                                     exception_state);
      if (exception_state.HadException()) {
        return ScriptPromise();
      }

      return SendDatagram(
          {static_cast<const uint8_t*>(data.View()->buffer()->Data()) +
               data.View()->byteOffsetAsSizeT(),
           data.View()->byteLengthAsSizeT()});
    }

    exception_state.ThrowTypeError(
        "Datagram is not an ArrayBuffer or ArrayBufferView type.");
    return ScriptPromise();
  }

  ScriptPromise close(ScriptState* script_state, ExceptionState&) override {
    quic_transport_ = nullptr;
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState&) override {
    quic_transport_ = nullptr;
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(quic_transport_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  ScriptPromise SendDatagram(base::span<const uint8_t> data) {
    if (!quic_transport_->quic_transport_) {
      // Silently drop the datagram if we are not connected.
      // TODO(ricea): Change the behaviour if the standard changes. See
      // https://github.com/WICG/web-transport/issues/93.
      return ScriptPromise::CastUndefined(quic_transport_->script_state_);
    }

    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        quic_transport_->script_state_);
    quic_transport_->quic_transport_->SendDatagram(
        data, WTF::Bind(&DatagramSent, WrapPersistent(resolver)));
    return resolver->Promise();
  }

  // |sent| indicates whether the datagram was sent or dropped. Currently we
  // |don't do anything with this information.
  static void DatagramSent(ScriptPromiseResolver* resolver, bool sent) {
    resolver->Resolve();
  }

  Member<QuicTransport> quic_transport_;
};

QuicTransport* QuicTransport::Create(ScriptState* script_state,
                                     const String& url,
                                     ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Create() url=" << url;
  auto* transport =
      MakeGarbageCollected<QuicTransport>(PassKey(), script_state, url);
  transport->Init(url, exception_state);
  return transport;
}

QuicTransport::QuicTransport(PassKey,
                             ScriptState* script_state,
                             const String& url)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      url_(NullURL(), url) {}

void QuicTransport::close(const WebTransportCloseInfo* close_info) {
  DVLOG(1) << "QuicTransport::close() this=" << this;
  // TODO(ricea): Send |close_info| to the network service.

  cleanly_closed_ = true;
  // If we don't manage to close the writable stream here, then it will
  // error when a write() is attempted.
  if (!WritableStream::IsLocked(outgoing_datagrams_) &&
      !WritableStream::CloseQueuedOrInFlight(outgoing_datagrams_)) {
    auto promise = WritableStream::Close(script_state_, outgoing_datagrams_);
    promise->MarkAsHandled();
  }
  Dispose();
}

void QuicTransport::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::QuicTransport> quic_transport,
    mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>
        client_receiver) {
  DVLOG(1) << "QuicTransport::OnConnectionEstablished() this=" << this;
  handshake_client_receiver_.reset();

  // TODO(ricea): Report to devtools.

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);

  client_receiver_.Bind(std::move(client_receiver), task_runner);
  client_receiver_.set_disconnect_handler(
      WTF::Bind(&QuicTransport::OnConnectionError, WrapWeakPersistent(this)));

  DCHECK(!quic_transport_);
  quic_transport_.Bind(std::move(quic_transport), task_runner);
}

QuicTransport::~QuicTransport() = default;

void QuicTransport::OnHandshakeFailed() {
  DVLOG(1) << "QuicTransport::OnHandshakeFailed() this=" << this;
  Dispose();
}

void QuicTransport::ContextDestroyed(ExecutionContext* execution_context) {
  DVLOG(1) << "QuicTransport::ContextDestroyed() this=" << this;
  Dispose();
}

bool QuicTransport::HasPendingActivity() const {
  DVLOG(1) << "QuicTransport::HasPendingActivity() this=" << this;
  return handshake_client_receiver_.is_bound() || client_receiver_.is_bound();
}

void QuicTransport::Trace(Visitor* visitor) {
  visitor->Trace(outgoing_datagrams_);
  visitor->Trace(script_state_);
  ContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void QuicTransport::Init(const String& url, ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Init() url=" << url << " this=" << this;
  if (!url_.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The URL '" + url + "' is invalid.");
    return;
  }

  if (!url_.ProtocolIs("quic-transport")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL's scheme must be 'quic-transport'. '" + url_.Protocol() +
            "' is not allowed.");
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('#" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in QuicTransport URLs.");
    return;
  }

  auto* execution_context = GetExecutionContext();

  if (!execution_context->GetContentSecurityPolicyForWorld()
           ->AllowConnectToSource(url_)) {
    // TODO(ricea): This error should probably be asynchronous like it is for
    // WebSockets and fetch.
    exception_state.ThrowSecurityError(
        "Failed to connect to '" + url_.ElidedString() + "'",
        "Refused to connect to '" + url_.ElidedString() +
            "' because it violates the document's Content Security Policy");
    return;
  }

  // TODO(ricea): Register SchedulingPolicy so that we don't get throttled and
  // to disable bfcache. Must be done before shipping.

  // TODO(ricea): Check the SubresourceFilter and fail asynchronously if
  // disallowed. Must be done before shipping.

  mojo::Remote<mojom::blink::QuicTransportConnector> connector;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      connector.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));

  connector->Connect(
      url_, handshake_client_receiver_.BindNewPipeAndPassRemote(
                execution_context->GetTaskRunner(TaskType::kNetworking)));

  handshake_client_receiver_.set_disconnect_handler(
      WTF::Bind(&QuicTransport::OnConnectionError, WrapWeakPersistent(this)));

  // TODO(ricea): Report something to devtools.

  outgoing_datagrams_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_, MakeGarbageCollected<DatagramUnderlyingSink>(this), 1);
}

void QuicTransport::Dispose() {
  DVLOG(1) << "QuicTransport::Dispose() this=" << this;
  quic_transport_.reset();
  handshake_client_receiver_.reset();
  client_receiver_.reset();
}

void QuicTransport::OnConnectionError() {
  DVLOG(1) << "QuicTransport::OnConnectionError() this=" << this;

  if (!cleanly_closed_) {
    v8::Local<v8::Value> reason = V8ThrowException::CreateTypeError(
        script_state_->GetIsolate(), "Connection lost.");
    WritableStreamDefaultController::Error(
        script_state_, outgoing_datagrams_->Controller(), reason);
  }

  Dispose();
}

}  // namespace blink

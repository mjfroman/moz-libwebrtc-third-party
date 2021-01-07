// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

#include <memory>

#include "base/optional.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_logger.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace media {
class GpuVideoAcceleratorFactories;
class VideoEncoder;
struct VideoEncoderOutput;
}  // namespace media

namespace blink {

class ExceptionState;
enum class DOMExceptionCode;
class VideoEncoderConfig;
class VideoEncoderInit;
class VideoEncoderEncodeOptions;
class Visitor;

class MODULES_EXPORT VideoEncoder final
    : public ScriptWrappable,
      public ActiveScriptWrappable<VideoEncoder>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoEncoder* Create(ScriptState*,
                              const VideoEncoderInit*,
                              ExceptionState&);
  VideoEncoder(ScriptState*, const VideoEncoderInit*, ExceptionState&);
  ~VideoEncoder() override;

  // video_encoder.idl implementation.
  int32_t encodeQueueSize();

  void encode(VideoFrame* frame,
              const VideoEncoderEncodeOptions*,
              ExceptionState&);

  void configure(const VideoEncoderConfig*, ExceptionState&);

  ScriptPromise flush(ExceptionState&);

  void reset(ExceptionState&);

  void close(ExceptionState&);

  String state() { return state_; }

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 private:
  enum class AccelerationPreference { kAllow, kDeny, kRequire };

  // TODO(ezemtsov): Replace this with a {Audio|Video}EncoderConfig.
  struct ParsedConfig final : public GarbageCollected<ParsedConfig> {
    media::VideoCodec codec;
    media::VideoCodecProfile profile;
    uint8_t level;
    media::VideoColorSpace color_space;

    AccelerationPreference acc_pref;

    media::VideoEncoder::Options options;
    String codec_string;

    void Trace(Visitor*) const {}
  };

  struct Request final : public GarbageCollected<Request> {
    enum class Type {
      // Configure an encoder from scratch, possibly replacing the existing one.
      kConfigure,
      // Adjust options in the already configured encoder.
      kReconfigure,
      kEncode,
      kFlush,
    };

    void Trace(Visitor*) const;

    Type type;
    // Current value of VideoEncoder.reset_count_ when request was created.
    uint32_t reset_count = 0;
    Member<VideoFrame> frame;                            // used by kEncode
    Member<const VideoEncoderEncodeOptions> encodeOpts;  // used by kEncode
    Member<ScriptPromiseResolver> resolver;              // used by kFlush
  };

  void CallOutputCallback(
      ParsedConfig* active_config,
      uint32_t reset_count,
      media::VideoEncoderOutput output,
      base::Optional<media::VideoEncoder::CodecDescription> codec_desc);
  void HandleError(DOMException* ex);
  void EnqueueRequest(Request* request);
  void ProcessRequests();
  void ProcessEncode(Request* request);
  void ProcessConfigure(Request* request);
  void ProcessReconfigure(Request* request);
  void ProcessFlush(Request* request);

  void UpdateEncoderLog(std::string encoder_name, bool is_hw_accelerated);

  void ResetInternal();
  ScriptPromiseResolver* MakePromise();

  void OnReceivedGpuFactories(Request*, media::GpuVideoAcceleratorFactories*);

  ParsedConfig* ParseConfig(const VideoEncoderConfig*, ExceptionState&);
  bool VerifyCodecSupport(ParsedConfig*, ExceptionState&);
  void CreateAndInitializeEncoderWithoutAcceleration(Request* request);
  void CreateAndInitializeEncoderOnEncoderSupportKnown(
      Request* request,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  std::unique_ptr<media::VideoEncoder> CreateMediaVideoEncoder(
      const ParsedConfig& config,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  bool CanReconfigure(ParsedConfig& original_config, ParsedConfig& new_config);
  scoped_refptr<media::VideoFrame> ReadbackTextureBackedFrameToMemory(
      scoped_refptr<media::VideoFrame> txt_frame);

  std::unique_ptr<CodecLogger> logger_;

  std::unique_ptr<media::VideoEncoder> media_encoder_;

  V8CodecState state_;

  Member<ParsedConfig> active_config_;
  Member<ScriptState> script_state_;
  Member<V8VideoEncoderOutputCallback> output_callback_;
  Member<V8WebCodecsErrorCallback> error_callback_;
  HeapDeque<Member<Request>> requests_;
  int32_t requested_encodes_ = 0;
  // How many times reset() was called on the encoder. It's used to decide
  // when a callback needs to be dismissed because reset() was called between
  // an operation and its callback.
  uint32_t reset_count_ = 0;

  // Some kConfigure and kFlush requests can't be executed in parallel with
  // kEncode. This flag stops processing of new requests in the requests_ queue
  // till the current requests are finished.
  bool stall_request_processing_ = false;
  media::VideoFramePool readback_frame_pool_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

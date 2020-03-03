// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace webrtc {
namespace video_coding {
class EncodedFrame;
}  // namespace video_coding
}  // namespace webrtc

namespace blink {

class DOMArrayBuffer;

class MODULES_EXPORT RTCEncodedVideoFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RTCEncodedVideoFrame(
      std::unique_ptr<webrtc::video_coding::EncodedFrame> delegate,
      std::vector<uint8_t> generic_descriptor);

  // rtc_encoded_video_frame.idl implementation.
  String type() const;
  uint64_t timestamp() const;
  DOMArrayBuffer* data() const;
  DOMArrayBuffer* additionalData() const;
  void setData(DOMArrayBuffer*);
  String toString() const;

  // Internal API
  bool HasDelegate() { return !!delegate_; }
  // Returns and transfers ownership of the internal WebRTC frame
  // backing this RTCEncodedVideoFrame, leaving the RTCEncodedVideoFrame
  // without a delegate WebRTC frame.
  std::unique_ptr<webrtc::video_coding::EncodedFrame> PassDelegate();

  void Trace(Visitor*) override;

 private:
  std::unique_ptr<webrtc::video_coding::EncodedFrame> delegate_;
  std::vector<uint8_t> additional_data_vector_;
  bool replaced_frame_data_ = false;
  // Exposes encoded frame data from |delegate_|.
  mutable Member<DOMArrayBuffer> frame_data_;
  // Exposes data from |additional_data_vector_|.
  mutable Member<DOMArrayBuffer> additional_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_FRAME_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_metadata.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture_enum.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WebGLVideoTexture::WebGLVideoTexture(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("WEBGL_video_texture");
}

WebGLExtensionName WebGLVideoTexture::GetName() const {
  return kWebGLVideoTextureName;
}

// We only need GL_OES_EGL_image_external extension on Android.
bool WebGLVideoTexture::Supported(WebGLRenderingContextBase* context) {
#if defined(OS_ANDROID)
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_EGL_image_external");
#else  // defined OS_ANDROID
  return true;
#endif
}

const char* WebGLVideoTexture::ExtensionName() {
  return "WEBGL_video_texture";
}

void WebGLVideoTexture::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_frame_metadata_);
  WebGLExtension::Trace(visitor);
}

VideoFrameMetadata* WebGLVideoTexture::VideoElementTargetVideoTexture(
    ExecutionContext* execution_context,
    unsigned target,
    HTMLVideoElement* video,
    ExceptionState& exceptionState) {
  WebGLExtensionScopedContext scoped(this);
  if (!video || scoped.IsLost())
    return nullptr;

  if (target != GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "WEBGLVideoTexture",
                                        "invalid texture target");
  }

  if (!scoped.Context()->ValidateHTMLVideoElement(
          execution_context->GetSecurityOrigin(), "WEBGLVideoTexture", video,
          exceptionState) ||
      !scoped.Context()->ValidateTexFuncDimensions(
          "WEBGLVideoTexture", WebGLRenderingContextBase::kTexImage, target, 0,
          video->videoWidth(), video->videoHeight(), 1))
    return nullptr;

  WebGLTexture* texture =
      scoped.Context()->ValidateTextureBinding("WEBGLVideoTexture", target);
  if (!texture)
    return nullptr;

  // For WebGL last-uploaded-frame-metadata API.
  WebMediaPlayer::VideoFrameUploadMetadata frame_metadata = {};
  int already_uploaded_id = HTMLVideoElement::kNoAlreadyUploadedFrame;
  WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr =
      &frame_metadata;
  if (RuntimeEnabledFeatures::ExtraWebGLVideoTextureMetadataEnabled()) {
    already_uploaded_id = texture->GetLastUploadedVideoFrameId();
  }

#if defined(OS_ANDROID)
  target = GL_TEXTURE_EXTERNAL_OES;
#else  // defined OS_ANDROID
  target = GL_TEXTURE_2D;

#endif  // defined OS_ANDROID

  video->PrepareVideoFrameForWebGL(scoped.Context()->ContextGL(), target,
                                   texture->Object(), already_uploaded_id,
                                   frame_metadata_ptr);
  if (!frame_metadata_ptr) {
    return nullptr;
  }

  if (frame_metadata_ptr) {
    current_frame_metadata_ = VideoFrameMetadata::Create();
    current_frame_metadata_->setPresentationTime(
        frame_metadata_ptr->timestamp.InMicrosecondsF());
    current_frame_metadata_->setExpectedPresentationTime(
        frame_metadata_ptr->expected_timestamp.InMicrosecondsF());
    current_frame_metadata_->setWidth(frame_metadata_ptr->visible_rect.width());
    current_frame_metadata_->setHeight(
        frame_metadata_ptr->visible_rect.height());
    current_frame_metadata_->setPresentationTimestamp(
        frame_metadata_ptr->timestamp.InSecondsF());
  }

  return current_frame_metadata_;
}

}  // namespace blink

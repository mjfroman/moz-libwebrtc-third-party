// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track_list.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace blink {

namespace {

media::VideoPixelFormat YUVSubsamplingToMediaPixelFormat(
    cc::YUVSubsampling sampling,
    int depth) {
  // TODO(crbug.com/1073995): Add support for high bit depth format.
  if (depth != 8)
    return media::PIXEL_FORMAT_UNKNOWN;

  switch (sampling) {
    case cc::YUVSubsampling::k420:
      return media::PIXEL_FORMAT_I420;
    case cc::YUVSubsampling::k422:
      return media::PIXEL_FORMAT_I422;
    case cc::YUVSubsampling::k444:
      return media::PIXEL_FORMAT_I444;
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

gfx::ColorSpace YUVColorSpaceToGfxColorSpace(SkYUVColorSpace yuv_cs,
                                             const gfx::ColorSpace& rgb_cs) {
  switch (yuv_cs) {
    case kJPEG_Full_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::SMPTE170M,
                                          gfx::ColorSpace::RangeID::FULL);
    case kRec601_Limited_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::SMPTE170M,
                                          gfx::ColorSpace::RangeID::LIMITED);
    case kRec709_Full_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::BT709,
                                          gfx::ColorSpace::RangeID::FULL);
    case kRec709_Limited_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::BT709,
                                          gfx::ColorSpace::RangeID::LIMITED);
    case kBT2020_8bit_Full_SkYUVColorSpace:
    case kBT2020_10bit_Full_SkYUVColorSpace:
    case kBT2020_12bit_Full_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::BT2020_NCL,
                                          gfx::ColorSpace::RangeID::FULL);
    case kBT2020_8bit_Limited_SkYUVColorSpace:
    case kBT2020_10bit_Limited_SkYUVColorSpace:
    case kBT2020_12bit_Limited_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::BT2020_NCL,
                                          gfx::ColorSpace::RangeID::LIMITED);
    case kIdentity_SkYUVColorSpace:
      return rgb_cs.GetWithMatrixAndRange(gfx::ColorSpace::MatrixID::GBR,
                                          gfx::ColorSpace::RangeID::FULL);
  };
}

bool IsTypeSupportedInternal(String type) {
  return type.ContainsOnlyASCIIOrEmpty() &&
         IsSupportedImageMimeType(type.Ascii());
}

ImageDecoder::AnimationOption AnimationOptionFromIsAnimated(bool is_animated) {
  return is_animated ? ImageDecoder::AnimationOption::kPreferAnimation
                     : ImageDecoder::AnimationOption::kPreferStillImage;
}

DOMException* CreateUnsupportedImageTypeException(String type) {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      String::Format("The provided image type (%s) is not supported",
                     type.Ascii().c_str()));
}

DOMException* CreateClosedException() {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kInvalidStateError, "The decoder has been closed.");
}

DOMException* CreateNoSelectedTracksException() {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kInvalidStateError, "No selected track.");
}

DOMException* CreateDecodeFailure(uint32_t index) {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kEncodingError,
      String::Format("Failed to decode frame at index %d", index));
}

}  // namespace

// static
ImageDecoderExternal* ImageDecoderExternal::Create(
    ScriptState* script_state,
    const ImageDecoderInit* init,
    ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<ImageDecoderExternal>(script_state, init,
                                                            exception_state);
  return exception_state.HadException() ? nullptr : result;
}

ImageDecoderExternal::DecodeRequest::DecodeRequest(
    ScriptPromiseResolver* resolver,
    uint32_t frame_index,
    bool complete_frames_only)
    : resolver(resolver),
      frame_index(frame_index),
      complete_frames_only(complete_frames_only) {}

void ImageDecoderExternal::DecodeRequest::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
  visitor->Trace(result);
  visitor->Trace(exception);
}

// static
ScriptPromise ImageDecoderExternal::isTypeSupported(ScriptState* script_state,
                                                    String type) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve(IsTypeSupportedInternal(type));
  return promise;
}

ImageDecoderExternal::ImageDecoderExternal(ScriptState* script_state,
                                           const ImageDecoderInit* init,
                                           ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      tracks_(MakeGarbageCollected<ImageTrackList>(this)) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);

  // |data| is a required field.
  DCHECK(init->hasData());
  DCHECK(!init->data().IsNull());

  constexpr char kNoneOption[] = "none";
  if (init->colorSpaceConversion() == kNoneOption)
    color_behavior_ = ColorBehavior::Ignore();
  if (init->premultiplyAlpha() == kNoneOption)
    alpha_option_ = ImageDecoder::kAlphaNotPremultiplied;
  if (init->hasDesiredWidth() && init->hasDesiredHeight())
    desired_size_ = SkISize::Make(init->desiredWidth(), init->desiredHeight());

  mime_type_ = init->type().LowerASCII();
  if (!IsTypeSupportedInternal(mime_type_))
    return;

  if (init->hasPreferAnimation()) {
    prefer_animation_ = init->preferAnimation();
    animation_option_ = AnimationOptionFromIsAnimated(*prefer_animation_);
  }

  if (init->data().IsReadableStream()) {
    if (init->data().GetAsReadableStream()->IsLocked() ||
        init->data().GetAsReadableStream()->IsDisturbed()) {
      exception_state.ThrowTypeError(
          "ImageDecoder can only accept readable streams that are not yet "
          "locked to a reader");
      return;
    }
    stream_buffer_ = WTF::SharedBuffer::Create();
    CreateImageDecoder();

    consumer_ = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state, init->data().GetAsReadableStream());

    // We need one initial call to OnStateChange() to start reading, but
    // thereafter calls will be driven by the ReadableStreamBytesConsumer.
    consumer_->SetClient(this);
    OnStateChange();
    return;
  }

  // Since we don't make a copy of buffer passed in, we must retain a reference.
  init_data_ = init;

  DOMArrayPiece buffer;
  if (init->data().IsArrayBuffer()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBuffer());
  } else if (init->data().IsArrayBufferView()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBufferView().Get());
  } else {
    NOTREACHED();
    return;
  }

  if (!buffer.ByteLength()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "No image data provided");
    return;
  }

  // Since data is owned by the caller who may be free to manipulate it, we must
  // check HasValidEncodedData() before attempting to access |decoder_|.
  segment_reader_ = SegmentReader::CreateFromSkData(
      SkData::MakeWithoutCopy(buffer.Data(), buffer.ByteLength()));
  if (!segment_reader_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Failed to read image data");
    return;
  }

  data_complete_ = true;

  CreateImageDecoder();
  MaybeUpdateMetadata();
  if (decoder_->Failed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Image decoding failed");
    return;
  }
}

ImageDecoderExternal::~ImageDecoderExternal() {
  DVLOG(1) << __func__;
}

ScriptPromise ImageDecoderExternal::decode(const ImageDecodeOptions* options) {
  DVLOG(1) << __func__;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();

  if (closed_) {
    resolver->Reject(CreateClosedException());
    return promise;
  }

  if (!decoder_) {
    resolver->Reject(IsTypeSupportedInternal(mime_type_)
                         ? CreateNoSelectedTracksException()
                         : CreateUnsupportedImageTypeException(mime_type_));
    return promise;
  }

  pending_decodes_.push_back(MakeGarbageCollected<DecodeRequest>(
      resolver, options ? options->frameIndex() : 0,
      options ? options->completeFramesOnly() : true));
  MaybeSatisfyPendingDecodes();
  return promise;
}

ScriptPromise ImageDecoderExternal::decodeMetadata() {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();

  if (closed_) {
    resolver->Reject(CreateClosedException());
    return promise;
  }

  if (!decoder_) {
    // We may have no selected tracks at this point.
    if (IsTypeSupportedInternal(mime_type_)) {
      DCHECK(pending_metadata_decodes_.IsEmpty());
      resolver->Resolve();
      return promise;
    }

    resolver->Reject(CreateUnsupportedImageTypeException(mime_type_));
    return promise;
  }

  pending_metadata_decodes_.push_back(resolver);
  MaybeSatisfyPendingMetadataDecodes();
  return promise;
}

void ImageDecoderExternal::UpdateSelectedTrack() {
  DCHECK(!closed_);

  reset(MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Aborted by track change"));

  // TODO(crbug.com/1073995): We eventually need a formal track selection
  // mechanism. For now we can only select between the still and animated images
  // and must destruct the decoder for changes.
  decoder_.reset();
  if (!tracks_->selectedTrack())
    return;

  animation_option_ = AnimationOptionFromIsAnimated(
      tracks_->selectedTrack().value()->animated());

  CreateImageDecoder();
  MaybeUpdateMetadata();
  MaybeSatisfyPendingDecodes();
}

String ImageDecoderExternal::type() const {
  return mime_type_;
}

bool ImageDecoderExternal::complete() const {
  return data_complete_;
}

ImageTrackList& ImageDecoderExternal::tracks() const {
  return *tracks_;
}

void ImageDecoderExternal::reset(DOMException* exception) {
  if (!exception) {
    exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Aborted by reset.");
  }

  // Move all state to local variables since promise resolution is reentrant.
  HeapVector<Member<ScriptPromiseResolver>> local_pending_metadata_decodes;
  local_pending_metadata_decodes.swap(pending_metadata_decodes_);

  AbortPendingDecodes(exception);
  for (auto& resolver : local_pending_metadata_decodes)
    resolver->Reject(exception);
}

void ImageDecoderExternal::close() {
  reset(MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Aborted by close."));
  if (consumer_)
    consumer_->Cancel();
  consumer_ = nullptr;
  decoder_.reset();
  tracks_->Disconnect();
  mime_type_ = "";
  stream_buffer_ = nullptr;
  segment_reader_ = nullptr;
  closed_ = true;
}

void ImageDecoderExternal::OnStateChange() {
  DCHECK(!closed_);
  DCHECK(consumer_);

  const char* buffer;
  size_t available;
  while (!data_complete_) {
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;

    if (result == BytesConsumer::Result::kOk) {
      if (available > 0)
        stream_buffer_->Append(buffer, SafeCast<wtf_size_t>(available));
      result = consumer_->EndRead(available);
    }

    data_complete_ = result == BytesConsumer::Result::kDone ||
                     result == BytesConsumer::Result::kError;
    decoder_->SetData(stream_buffer_, data_complete_);
    MaybeUpdateMetadata();
    MaybeSatisfyPendingDecodes();
  }
}

String ImageDecoderExternal::DebugName() const {
  return "ImageDecoderExternal";
}

void ImageDecoderExternal::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(consumer_);
  visitor->Trace(tracks_);
  visitor->Trace(pending_decodes_);
  visitor->Trace(pending_metadata_decodes_);
  visitor->Trace(init_data_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ImageDecoderExternal::ContextDestroyed() {}

bool ImageDecoderExternal::HasPendingActivity() const {
  return !pending_metadata_decodes_.IsEmpty() || !pending_decodes_.IsEmpty();
}

void ImageDecoderExternal::CreateImageDecoder() {
  DCHECK(!decoder_);
  DCHECK(HasValidEncodedData());

  // TODO(crbug.com/1073995): We should probably call
  // ImageDecoder::SetMemoryAllocator() so that we can recycle frame buffers for
  // decoded images.

  if (stream_buffer_) {
    if (!segment_reader_)
      segment_reader_ = SegmentReader::CreateFromSharedBuffer(stream_buffer_);
  } else {
    DCHECK(data_complete_);
  }

  DCHECK(IsTypeSupportedInternal(mime_type_));
  decoder_ = ImageDecoder::CreateByMimeType(
      mime_type_, segment_reader_, data_complete_, alpha_option_,
      ImageDecoder::kDefaultBitDepth, color_behavior_, desired_size_,
      animation_option_);

  // CreateByImageType() can't fail if we use a supported image type. Which we
  // DCHECK above via isTypeSupported().
  DCHECK(decoder_) << mime_type_;
}

void ImageDecoderExternal::MaybeSatisfyPendingDecodes() {
  DCHECK(!closed_);
  DCHECK(decoder_);

  for (auto& request : pending_decodes_) {
    if (decoder_->Failed()) {
      request->exception = CreateDecodeFailure(request->frame_index);
      continue;
    }
    if (!data_complete_) {
      // We can't fulfill this promise at this time.
      if (tracks_->IsEmpty() ||
          request->frame_index >=
              tracks_->selectedTrack().value()->frameCount()) {
        continue;
      }
    } else if (request->frame_index >=
               tracks_->selectedTrack().value()->frameCount()) {
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kIndexSizeError,
          ExceptionMessages::IndexOutsideRange<uint32_t>(
              "frame index", request->frame_index, 0,
              ExceptionMessages::kInclusiveBound,
              tracks_->selectedTrack().value()->frameCount(),
              ExceptionMessages::kExclusiveBound));
      continue;
    }

    if (!HasValidEncodedData()) {
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Source data has been neutered");
      continue;
    }

    bool is_complete = true;
    sk_sp<SkImage> sk_image;
    scoped_refptr<media::VideoFrame> frame;

    // Due to implementation limitations YUV support for some formats is only
    // known once all data is received. Animated images are never supported.
    if (decoder_->CanDecodeToYUV()) {
      DCHECK(!tracks_->selectedTrack().value()->animated());
      DCHECK_EQ(request->frame_index, 0u);
      frame = MaybeDecodeToYuv();
      if (decoder_->Failed()) {
        request->exception = CreateDecodeFailure(request->frame_index);
        continue;
      }
    }

    if (!frame) {
      auto* image = decoder_->DecodeFrameBufferAtIndex(request->frame_index);
      if (decoder_->Failed() || !image) {
        request->exception = CreateDecodeFailure(request->frame_index);
        continue;
      }

      // Only satisfy fully complete decode requests.
      is_complete = image->GetStatus() == ImageFrame::kFrameComplete;
      if (!is_complete && request->complete_frames_only)
        continue;

      if (!is_complete && image->GetStatus() != ImageFrame::kFramePartial)
        continue;

      // Prefer FinalizePixelsAndGetImage() since that will mark the underlying
      // bitmap as immutable, which allows copies to be avoided.
      sk_image = is_complete ? image->FinalizePixelsAndGetImage()
                             : SkImage::MakeFromBitmap(image->Bitmap());
      if (!sk_image) {
        request->exception = MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kOperationError, "Failed to access frame");
        continue;
      }

      if (!is_complete) {
        auto generation_id = image->Bitmap().getGenerationID();
        auto it = incomplete_frames_.find(request->frame_index);
        if (it == incomplete_frames_.end()) {
          incomplete_frames_.Set(request->frame_index, generation_id);
        } else {
          // Don't fulfill the promise until a new bitmap is seen.
          if (it->value == generation_id)
            continue;

          it->value = generation_id;
        }
      } else {
        incomplete_frames_.erase(request->frame_index);
      }

      // TODO(crbug.com/1073995): Add timestamp support to ImageDecoder if we
      // end up encountering a lot of variable duration images.
      const auto timestamp =
          decoder_->FrameDurationAtIndex(request->frame_index) *
          request->frame_index;

      // This is zero copy; the VideoFrame points into the SkBitmap.
      const gfx::Size coded_size(sk_image->width(), sk_image->height());
      frame = media::CreateFromSkImage(sk_image, gfx::Rect(coded_size),
                                       coded_size, timestamp);
    }

    if (!frame) {
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Failed to create frame");
      continue;
    }

    frame->metadata().transformation = ImageOrientationToVideoTransformation(
        decoder_->Orientation().Orientation());
    frame->metadata().frame_duration =
        decoder_->FrameDurationAtIndex(request->frame_index);

    request->result = ImageDecodeResult::Create();
    request->result->setImage(
        MakeGarbageCollected<VideoFrame>(base::MakeRefCounted<VideoFrameHandle>(
            std::move(frame), std::move(sk_image))));
    request->result->setComplete(is_complete);
  }

  auto* new_end =
      std::stable_partition(pending_decodes_.begin(), pending_decodes_.end(),
                            [](const auto& request) {
                              return !request->result && !request->exception;
                            });

  // Copy completed requests to a new local vector to avoid reentrancy issues
  // when resolving and rejecting the promises.
  HeapVector<Member<DecodeRequest>> completed_decodes;
  completed_decodes.AppendRange(new_end, pending_decodes_.end());
  pending_decodes_.Shrink(
      static_cast<wtf_size_t>(new_end - pending_decodes_.begin()));

  // Note: Promise resolution may invoke calls into this class.
  for (auto& request : completed_decodes) {
    if (request->exception)
      request->resolver->Reject(request->exception);
    else
      request->resolver->Resolve(request->result);
  }
}

void ImageDecoderExternal::MaybeSatisfyPendingMetadataDecodes() {
  DCHECK(decoder_);
  DCHECK(!closed_);
  DCHECK(HasValidEncodedData());
  if (!decoder_->IsSizeAvailable() && !decoder_->Failed())
    return;

  DCHECK(decoder_->Failed() || decoder_->IsDecodedSizeAvailable());
  for (auto& resolver : pending_metadata_decodes_) {
    if (decoder_->Failed())
      resolver->Reject();
    else
      resolver->Resolve();
  }
  pending_metadata_decodes_.clear();
}

void ImageDecoderExternal::MaybeUpdateMetadata() {
  DCHECK(decoder_);
  DCHECK(!closed_);

  if (!HasValidEncodedData())
    return;

  // Since we always create the decoder at construction, we need to wait until
  // at least the size is available before signaling that metadata has been
  // retrieved.
  if (!decoder_->IsSizeAvailable() || decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  uint32_t frame_count = static_cast<uint32_t>(decoder_->FrameCount());
  if (decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  int repetition_count = decoder_->RepetitionCount();
  if (decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  if (tracks_->IsEmpty()) {
    // TODO(crbug.com/1073995): None of the underlying ImageDecoders actually
    // expose tracks yet. So for now just assume a still and animated track for
    // images which declare to be multi-image and have animations.

    if (decoder_->ImageHasBothStillAndAnimatedSubImages()) {
      int selected_track_id = 1;  // Currently animation is always default.
      if (prefer_animation_.has_value()) {
        selected_track_id = prefer_animation_.value() ? 1 : 0;

        // Sadly there's currently no way to get the frame count information for
        // unselected tracks, so for now just leave frame count as unknown but
        // force repetition count to be animated.
        if (!prefer_animation_.value()) {
          frame_count = 0;
          repetition_count = kAnimationLoopOnce;
        }
      }

      // All multi-track images have a still image track. Even if it's just the
      // first frame of the animation.
      tracks_->AddTrack(1, kAnimationNone, selected_track_id == 0);
      tracks_->AddTrack(frame_count, repetition_count, selected_track_id == 1);
    } else {
      tracks_->AddTrack(frame_count, repetition_count, true);
    }
  } else {
    tracks_->selectedTrack().value()->UpdateTrack(frame_count,
                                                  repetition_count);
  }

  MaybeSatisfyPendingMetadataDecodes();
}

bool ImageDecoderExternal::HasValidEncodedData() const {
  // If we keep an internal copy of the data, it's always valid.
  if (stream_buffer_)
    return true;

  if (init_data_->data().IsArrayBuffer() &&
      init_data_->data().GetAsArrayBuffer()->IsDetached()) {
    return false;
  }

  if (init_data_->data().IsArrayBufferView() &&
      !init_data_->data().GetAsArrayBufferView()->BaseAddress()) {
    return false;
  }

  return true;
}

void ImageDecoderExternal::AbortPendingDecodes(DOMException* exception) {
  // Swap to a local variable since promise handling may be reentrant.
  HeapVector<Member<DecodeRequest>> local_pending_decodes;
  local_pending_decodes.swap(pending_decodes_);
  for (auto& request : local_pending_decodes)
    request->resolver->Reject(exception);
  incomplete_frames_.clear();
}

scoped_refptr<media::VideoFrame> ImageDecoderExternal::MaybeDecodeToYuv() {
  const auto format = YUVSubsamplingToMediaPixelFormat(
      decoder_->GetYUVSubsampling(), decoder_->GetYUVBitDepth());
  if (format == media::PIXEL_FORMAT_UNKNOWN)
    return nullptr;

  const auto coded_size = gfx::Size(decoder_->DecodedYUVSize(cc::YUVIndex::kY));

  // Plane sizes are guaranteed to fit in an int32_t by ImageDecoder::SetSize();
  // since YUV is 1 byte-per-channel, we can just check width * height.
  DCHECK(coded_size.GetCheckedArea().IsValid());
  auto layout = media::VideoFrameLayout::CreateWithStrides(
      format, coded_size,
      {static_cast<int32_t>(decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kY)),
       static_cast<int32_t>(decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kU)),
       static_cast<int32_t>(decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kV))});
  if (!layout)
    return nullptr;

  auto frame = media::VideoFrame::CreateFrameWithLayout(
      *layout, gfx::Rect(coded_size), coded_size, base::TimeDelta(),
      /*zero_initialize_memory=*/false);
  if (!frame)
    return nullptr;

  void* planes[cc::kNumYUVPlanes] = {frame->data(0), frame->data(1),
                                     frame->data(2)};
  size_t row_bytes[cc::kNumYUVPlanes] = {frame->stride(0), frame->stride(1),
                                         frame->stride(2)};

  // TODO(crbug.com/1073995): Add support for high bit depth format.
  const auto color_type = kGray_8_SkColorType;

  auto image_planes =
      std::make_unique<ImagePlanes>(planes, row_bytes, color_type);
  decoder_->SetImagePlanes(std::move(image_planes));
  decoder_->DecodeToYUV();
  if (decoder_->Failed() || !decoder_->HasDisplayableYUVData())
    return nullptr;

  frame->set_color_space(YUVColorSpaceToGfxColorSpace(
      decoder_->GetYUVColorSpace(),
      gfx::ColorSpace(*decoder_->ColorSpaceForSkImages())));

  return frame;
}

}  // namespace blink

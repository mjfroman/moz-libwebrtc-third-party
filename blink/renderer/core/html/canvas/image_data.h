/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/uint8_clamped_array_or_uint16_array_or_float32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data_settings.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace blink {

class ExceptionState;
class ImageBitmapOptions;

typedef Uint8ClampedArrayOrUint16ArrayOrFloat32Array ImageDataArray;

enum ConstructorParams {
  kParamSize = 1,
  kParamWidth = 1 << 1,
  kParamHeight = 1 << 2,
  kParamData = 1 << 3,
};

constexpr const char* kUint8ClampedArrayStorageFormatName = "uint8";
constexpr const char* kUint16ArrayStorageFormatName = "uint16";
constexpr const char* kFloat32ArrayStorageFormatName = "float32";

class CORE_EXPORT ImageData final : public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageData* Create(const IntSize&, const ImageDataSettings* = nullptr);
  static ImageData* Create(const IntSize&,
                           CanvasColorSpace,
                           ImageDataStorageFormat);
  static ImageData* Create(const IntSize&,
                           NotShared<DOMArrayBufferView>,
                           const ImageDataSettings* = nullptr);

  static ImageData* Create(unsigned width, unsigned height, ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           unsigned height,
                           ExceptionState&);

  static ImageData* CreateImageData(unsigned width,
                                    unsigned height,
                                    const ImageDataSettings*,
                                    ExceptionState&);
  static ImageData* CreateImageData(ImageDataArray&,
                                    unsigned width,
                                    unsigned height,
                                    ImageDataSettings*,
                                    ExceptionState&);

  ImageDataSettings* getSettings() { return settings_; }

  static ImageData* CreateForTest(const IntSize&);
  static ImageData* CreateForTest(const IntSize&,
                                  NotShared<DOMArrayBufferView>,
                                  const ImageDataSettings* = nullptr);

  ImageData(const IntSize&,
            NotShared<DOMArrayBufferView>,
            const ImageDataSettings* = nullptr);

  ImageData* CropRect(const IntRect&, bool flip_y = false);

  static String CanvasColorSpaceName(CanvasColorSpace);
  static ImageDataStorageFormat GetImageDataStorageFormat(const String&);
  static unsigned StorageFormatBytesPerPixel(const String&);
  static unsigned StorageFormatBytesPerPixel(ImageDataStorageFormat);

  IntSize Size() const { return size_; }
  int width() const { return size_.Width(); }
  int height() const { return size_.Height(); }

  ImageDataArray& data() { return data_; }
  const ImageDataArray& data() const { return data_; }
  void data(ImageDataArray& result) { result = data_; }

  DOMArrayBufferBase* BufferBase() const;
  CanvasColorSpace GetCanvasColorSpace() const;
  ImageDataStorageFormat GetImageDataStorageFormat() const;

  // Return an SkPixmap that references this data directly.
  SkPixmap GetSkPixmap() const;

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return size_; }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  void Trace(Visitor*) const override;

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

 private:
  IntSize size_;
  Member<ImageDataSettings> settings_;
  ImageDataArray data_;
  NotShared<DOMUint8ClampedArray> data_u8_;
  NotShared<DOMUint16Array> data_u16_;
  NotShared<DOMFloat32Array> data_f32_;

  static bool ValidateConstructorArguments(
      const IntSize* size,
      const unsigned* width,
      const unsigned* height,
      const NotShared<DOMArrayBufferView>* data,
      const ImageDataSettings* settings,
      ExceptionState* = nullptr);

  static NotShared<DOMArrayBufferView> AllocateAndValidateDataArray(
      const unsigned&,
      ImageDataStorageFormat,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_IMAGE_DATA_H_

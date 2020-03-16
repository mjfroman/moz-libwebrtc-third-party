/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_VIEW_H_

#include <limits.h>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class CORE_EXPORT ArrayBufferView : public RefCounted<ArrayBufferView> {
  USING_FAST_MALLOC(ArrayBuffer);

 public:
  enum ViewType {
    kTypeInt8,
    kTypeUint8,
    kTypeUint8Clamped,
    kTypeInt16,
    kTypeUint16,
    kTypeInt32,
    kTypeUint32,
    kTypeFloat32,
    kTypeFloat64,
    kTypeBigInt64,
    kTypeBigUint64,
    kTypeDataView
  };
  virtual ViewType GetType() const = 0;

  ArrayBuffer* Buffer() const { return buffer_.get(); }

  void* BaseAddress() const {
    DCHECK(!IsShared());
    return BaseAddressMaybeShared();
  }
  void* BaseAddressMaybeShared() const {
    return !IsDetached() ? raw_base_address_ : nullptr;
  }

  size_t ByteOffset() const { return !IsDetached() ? raw_byte_offset_ : 0; }

  virtual size_t ByteLengthAsSizeT() const = 0;
  virtual unsigned TypeSize() const = 0;

  bool IsShared() const { return buffer_ ? buffer_->IsShared() : false; }

  virtual ~ArrayBufferView() = default;

 protected:
  ArrayBufferView(scoped_refptr<ArrayBuffer> buffer, size_t byte_offset)
      : raw_byte_offset_(byte_offset), buffer_(std::move(buffer)) {
    raw_base_address_ =
        buffer_ ? (static_cast<char*>(buffer_->DataMaybeShared()) + byte_offset)
                : nullptr;
  }

  bool IsDetached() const { return !buffer_ || buffer_->IsDetached(); }

 private:
  // The raw_* fields may be stale after Detach. Use getters instead.
  // This is the address of the ArrayBuffer's storage, plus the byte offset.
  void* raw_base_address_;
  size_t raw_byte_offset_;

  scoped_refptr<ArrayBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_VIEW_H_

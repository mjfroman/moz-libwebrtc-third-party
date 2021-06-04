// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/blink_storage_key_mojom_traits.h"

#include "third_party/blink/renderer/platform/mojo/security_origin_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::StorageKeyDataView, blink::BlinkStorageKey>::
    Read(blink::mojom::StorageKeyDataView data, blink::BlinkStorageKey* out) {
  scoped_refptr<const blink::SecurityOrigin> origin;
  if (!data.ReadOrigin(&origin))
    return false;
  DCHECK(origin);

  *out = blink::BlinkStorageKey(std::move(origin));
  return true;
}

}  // namespace mojo

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ScrollSnapshotClient::ScrollSnapshotClient(LocalFrame* frame) {
  if (frame)
    frame->AddScrollSnapshotClient(*this);
}

bool ScrollSnapshotClient::ValidateSnapshotIfNeeded() {
  bool valid = true;
  if (force_validation_ || CheckIfNeedsValidation()) {
    valid = ValidateSnapshot();
  }
  force_validation_ = false;
  return valid;
}

}  // namespace blink

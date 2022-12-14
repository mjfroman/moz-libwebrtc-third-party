// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"

namespace blink {

TrustedHTML::TrustedHTML(String html) : html_(std::move(html)) {}

const String& TrustedHTML::toString() const {
  return html_;
}

}  // namespace blink

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"

namespace blink {

TrustedScriptURL::TrustedScriptURL(String url) : url_(std::move(url)) {}

const String& TrustedScriptURL::toString() const {
  return url_;
}

}  // namespace blink

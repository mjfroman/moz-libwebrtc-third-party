// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_response.h"

namespace blink {

AuthenticatorResponse::AuthenticatorResponse(DOMArrayBuffer* client_data_json)
    : client_data_json_(client_data_json) {}

AuthenticatorResponse::~AuthenticatorResponse() = default;

void AuthenticatorResponse::Trace(Visitor* visitor) const {
  visitor->Trace(client_data_json_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

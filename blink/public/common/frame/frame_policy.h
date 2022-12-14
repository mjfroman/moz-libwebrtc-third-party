// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_

#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"

namespace blink {

// This structure contains the attributes of a frame which determine what
// features are available during the lifetime of the framed document. Currently,
// this includes the sandbox flags, the permissions policy container policy, and
// document policy required policy. Used in the frame tree to track sandbox,
// permissions policy and document policy in the browser process, and
// transferred over IPC during frame replication when site isolation is enabled.
//
// Unlike the attributes in FrameOwnerProperties, these attributes are never
// updated after the framed document has been loaded, so two versions of this
// structure are kept in the frame tree for each frame -- the effective policy
// and the pending policy, which will take effect when the frame is next
// navigated.
struct BLINK_COMMON_EXPORT FramePolicy {
  FramePolicy();
  FramePolicy(network::mojom::WebSandboxFlags sandbox_flags,
              const ParsedPermissionsPolicy& container_policy,
              const DocumentPolicyFeatureState& required_document_policy);
  FramePolicy(const FramePolicy& lhs);
  ~FramePolicy();

  network::mojom::WebSandboxFlags sandbox_flags;
  ParsedPermissionsPolicy container_policy;
  // |required_document_policy| is the combination of the following:
  // - iframe 'policy' attribute
  // - 'Require-Document-Policy' http header
  // - |required_document_policy| of parent frame
  DocumentPolicyFeatureState required_document_policy;

  // This signals to a frame whether or not it is hosted in a <fencedframe>
  // element, and therefore should restrict some sensitive Window APIs that
  // would otherwise grant access to the parent frame, or information about it.
  //
  // IMPORTANT NOTE: The fenced frame members below are temporary and do not
  // align with the long-term architecture, and will be removed after the
  // <fencedframe> ShadowDOM-based origin trial, please do not use these for
  // anything else. See
  // https://docs.google.com/document/d/1ijTZJT3DHQ1ljp4QQe4E4XCCRaYAxmInNzN1SzeJM8s/edit
  // and crbug.com/1123606. Note that these bits are immutable and cannot
  // experience transitions, therefore `FramePolicy` is not actually a good
  // place for this bit to live, and would be best suited to be manually plumbed
  // through like `TreeScopeType`. However since this bit is temporary, any
  // plumbing we introduce for it will be thrown away once we migrate to the
  // long-term MPArch architecture, so we're using `FramePolicy` to avoid
  // temporarily investing in the manual plumbing that will not stick around.
  bool is_fenced = false;
  blink::mojom::FencedFrameMode fenced_frame_mode =
      blink::mojom::FencedFrameMode::kDefault;
};

bool BLINK_COMMON_EXPORT operator==(const FramePolicy& lhs,
                                    const FramePolicy& rhs);
bool BLINK_COMMON_EXPORT operator!=(const FramePolicy& lhs,
                                    const FramePolicy& rhs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_

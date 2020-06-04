// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"

#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRHitTestResult::XRHitTestResult(
    XRSession* session,
    const device::mojom::blink::XRHitResult& hit_result)
    : session_(session),
      mojo_from_this_(std::make_unique<TransformationMatrix>(
          hit_result.hit_matrix.matrix())),
      plane_id_(hit_result.plane_id != 0
                    ? base::Optional<uint64_t>(hit_result.plane_id)
                    : base::nullopt) {}

XRPose* XRHitTestResult::getPose(XRSpace* other) {
  auto maybe_other_space_native_from_mojo = other->NativeFromMojo();
  DCHECK(maybe_other_space_native_from_mojo);

  auto mojo_from_this = *mojo_from_this_;

  auto other_native_from_mojo = *maybe_other_space_native_from_mojo;
  auto other_offset_from_other_native = other->OffsetFromNativeMatrix();

  auto other_offset_from_mojo =
      other_offset_from_other_native * other_native_from_mojo;

  auto other_offset_from_this = other_offset_from_mojo * mojo_from_this;

  return MakeGarbageCollected<XRPose>(other_offset_from_this, false);
}

ScriptPromise XRHitTestResult::createAnchor(ScriptState* script_state,
                                            XRRigidTransform* this_from_anchor,
                                            ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!session_->IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      XRSession::kAnchorsFeatureNotSupported);
    return {};
  }

  if (!this_from_anchor) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kNoRigidTransformSpecified);
    return {};
  }

  if (plane_id_) {
    DVLOG(2) << __func__
             << ": hit test result's entity is a plane, creating "
                "plane-attached anchor";
    return session_->CreatePlaneAnchorHelper(
        script_state, this_from_anchor->TransformMatrix(), *plane_id_,
        exception_state);
  } else {
    DVLOG(2) << __func__
             << ": hit test result's entity is unavailable, creating "
                "free-floating anchor ";

    // Let's create free-floating anchor since plane is unavailable. Grab an
    // information about reference space that is well-suited for anchor creation
    // from session:
    base::Optional<XRSession::ReferenceSpaceInformation>
        reference_space_information = session_->GetStationaryReferenceSpace();

    if (!reference_space_information) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        XRSession::kUnableToRetrieveMatrix);
      return {};
    }

    const TransformationMatrix& mojo_from_space =
        reference_space_information->mojo_from_space;

    DCHECK(mojo_from_space.IsInvertible());

    auto space_from_mojo = mojo_from_space.Inverse();
    auto space_from_anchor = space_from_mojo * (*mojo_from_this_) *
                             this_from_anchor->TransformMatrix();

    return session_->CreateAnchorHelper(
        script_state, space_from_anchor,
        reference_space_information->native_origin, exception_state);
  }
}

void XRHitTestResult::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

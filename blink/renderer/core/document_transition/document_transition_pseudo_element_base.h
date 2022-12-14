// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_PSEUDO_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_PSEUDO_ELEMENT_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {
class DocumentTransitionStyleTracker;

class CORE_EXPORT DocumentTransitionPseudoElementBase : public PseudoElement {
 public:
  DocumentTransitionPseudoElementBase(
      Element* parent,
      PseudoId,
      const AtomicString& document_transition_tag,
      const DocumentTransitionStyleTracker* style_tracker);
  ~DocumentTransitionPseudoElementBase() override = default;

  bool CanGeneratePseudoElement(PseudoId) const override;
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject(
      const StyleRecalcContext&) override;
  void Trace(Visitor* visitor) const override;

 protected:
  Member<const DocumentTransitionStyleTracker> style_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_PSEUDO_ELEMENT_BASE_H_

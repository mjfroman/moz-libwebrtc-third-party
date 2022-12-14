// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_GRAMMAR_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_GRAMMAR_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"

namespace blink {

// A subclass of DocumentMarker used to store information specific to grammar
// markers. Spelling and grammar markers are identical except that they mark
// either spelling or grammar errors, respectively, so nearly all functionality
// is delegated to a common base class, SpellCheckMarker.
class CORE_EXPORT GrammarMarker final : public SpellCheckMarker {
 public:
  GrammarMarker(unsigned start_offset,
                unsigned end_offset,
                const String& description);
  GrammarMarker(const GrammarMarker&) = delete;
  GrammarMarker& operator=(const GrammarMarker&) = delete;

 private:
  // DocumentMarker implementations
  MarkerType GetType() const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_GRAMMAR_MARKER_H_

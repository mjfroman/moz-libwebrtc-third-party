// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSAnimation::CSSAnimation(ExecutionContext* execution_context,
                           AnimationTimeline* timeline,
                           AnimationEffect* content,
                           const String& animation_name)
    : Animation(execution_context, timeline, content),
      animation_name_(animation_name),
      ignore_css_play_state_(false) {}

String CSSAnimation::playState() const {
  FlushStyles();
  return Animation::playState();
}

bool CSSAnimation::pending() const {
  FlushStyles();
  return Animation::pending();
}

void CSSAnimation::pause(ExceptionState& exception_state) {
  Animation::pause(exception_state);
  if (exception_state.HadException())
    return;
  ignore_css_play_state_ = true;
}

void CSSAnimation::play(ExceptionState& exception_state) {
  Animation::play(exception_state);
  if (exception_state.HadException())
    return;
  ignore_css_play_state_ = true;
}

void CSSAnimation::reverse(ExceptionState& exception_state) {
  PlayStateTransitionScope scope(*this);
  Animation::reverse(exception_state);
}

void CSSAnimation::setStartTime(double start_time,
                                bool is_null,
                                ExceptionState& exception_state) {
  PlayStateTransitionScope scope(*this);
  Animation::setStartTime(start_time, is_null, exception_state);
}

void CSSAnimation::FlushStyles() const {
  // TODO(1043778): Flush is likely not required once the CSSAnimation is
  // disassociated from its owning element.
  if (GetDocument())
    GetDocument()->UpdateStyleAndLayoutTree();
}

CSSAnimation::PlayStateTransitionScope::PlayStateTransitionScope(
    CSSAnimation& animation)
    : animation_(animation) {
  was_paused_ = animation_.Paused();
}

CSSAnimation::PlayStateTransitionScope::~PlayStateTransitionScope() {
  bool is_paused = animation_.Paused();
  if (was_paused_ != is_paused)
    animation_.ignore_css_play_state_ = true;
}

}  // namespace blink

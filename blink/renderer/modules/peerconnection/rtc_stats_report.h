// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_STATS_REPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_STATS_REPORT_H_

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/maplike.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

// Returns the group ids for non-standardized members which should be exposed
// based on what Origin Trials are running.
Vector<webrtc::NonStandardGroupId> GetExposedGroupIds(
    const ScriptState* script_state);

// https://w3c.github.io/webrtc-pc/#rtcstatsreport-object
class RTCStatsReport final
    : public ScriptWrappable,
      public Maplike<String, IDLString, v8::Local<v8::Object>, IDLObject> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCStatsReport(std::unique_ptr<RTCStatsReportPlatform>);

  uint32_t size() const;

  // Maplike<String, v8::Local<v8::Value>>
  PairIterable<String, IDLString, v8::Local<v8::Object>, IDLObject>::
      IterationSource*
      StartIteration(ScriptState*, ExceptionState&) override;
  bool GetMapEntry(ScriptState*,
                   const String& key,
                   v8::Local<v8::Object>&,
                   ExceptionState&) override;

 private:
  std::unique_ptr<RTCStatsReportPlatform> report_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_STATS_REPORT_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/public/mojom/oom_intervention/oom_intervention.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class OomInterventionImplTest;

// Implementation of OOM intervention. This pauses all pages by using
// ScopedPagePauser when near-OOM situation is detected.
class CONTROLLER_EXPORT OomInterventionImpl
    : public mojom::blink::OomIntervention,
      public MemoryUsageMonitor::Observer {
 public:
  static void BindReceiver(
      mojo::PendingReceiver<mojom::blink::OomIntervention> receiver);

  OomInterventionImpl();
  ~OomInterventionImpl() override;

  void Reset();

  // mojom::blink::OomIntervention:
  void StartDetection(
      mojo::PendingRemote<mojom::blink::OomInterventionHost> host,
      mojom::blink::DetectionArgsPtr detection_args,
      bool renderer_pause_enabled,
      bool navigate_ads_enabled,
      bool purge_v8_memory_enabled) override;

  // MemoryUsageMonitor::Observer:
  void OnMemoryPing(MemoryUsage) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, DetectedAndDeclined);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, StopWatchingAfterDetection);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest,
                           ContinueWatchingWithoutDetection);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, V1DetectionAdsNavigation);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, MojoDisconnection);

  // Overridden by test.
  virtual MemoryUsageMonitor& MemoryUsageMonitorInstance();

  void Bind(mojo::PendingReceiver<mojom::blink::OomIntervention> receiver);

  void Check(MemoryUsage);

  void ReportMemoryStats(OomInterventionMetrics& current_memory);

  void TimerFiredUMAReport(TimerBase*);

  static void TriggerGC();

  mojom::blink::DetectionArgsPtr detection_args_;

  mojo::Remote<mojom::blink::OomInterventionHost> host_;
  bool renderer_pause_enabled_ = false;
  bool navigate_ads_enabled_ = false;
  bool purge_v8_memory_enabled_ = false;
  std::unique_ptr<ScopedPagePauser> pauser_;
  mojo::Receiver<mojom::blink::OomIntervention> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_

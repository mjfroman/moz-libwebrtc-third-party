// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/metrics/post_message_counter.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

namespace {
constexpr size_t kMaxRecordedPostsSize = 20;
}  // namespace

namespace blink {

bool PostMessageCounter::RecordMessageAndCheckIfShouldSend(
    ukm::SourceId source_id,
    const StorageKey& source_storage_key,
    ukm::SourceId target_id,
    const StorageKey& target_storage_key,
    ukm::UkmRecorder* recorder) {
  DCHECK_LE(recorded_posts_.size(), kMaxRecordedPostsSize);
  std::pair<ukm::SourceId, ukm::SourceId> new_pair(source_id, target_id);

  // We want these storage keys to behave as though storage partitioning is on
  // for convenience.
  const auto& source_3psp_key =
      source_storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();
  const auto& target_3psp_key =
      target_storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();

  // If the feature is on, we want to block postMessages between the same
  // origin on different top level sites (cross-partition).
  bool should_send_post_message =
      source_3psp_key == target_3psp_key || source_3psp_key.origin().opaque() ||
      target_3psp_key.origin().opaque() ||
      source_3psp_key.origin() != target_3psp_key.origin() ||
      !base::FeatureList::IsEnabled(
          features::kPostMessageDifferentPartitionSameOriginBlocked);

  if (base::Contains(recorded_posts_, new_pair)) {
    return should_send_post_message;
  }
  if (recorded_posts_.size() == kMaxRecordedPostsSize)
    recorded_posts_.pop_back();
  recorded_posts_.push_front(new_pair);

  switch (partition_) {
    case PostMessagePartition::kCrossProcess: {
      ukm::builders::PostMessage_Incoming_Page builder(target_id);
      builder.SetSourcePageSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessagePartition::kSameProcess: {
      ukm::builders::PostMessage_Incoming_Frame builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
  }

  if (source_3psp_key.origin().opaque() ||
      source_3psp_key.top_level_site().opaque() ||
      target_3psp_key.origin().opaque() ||
      target_3psp_key.top_level_site().opaque()) {
    ukm::builders::PostMessage_Incoming_Opaque builder(target_id);
    builder.SetSourceFrameSourceId(source_id);
    builder.Record(recorder);
  } else if (source_3psp_key.IsFirstPartyContext() &&
             target_3psp_key.IsFirstPartyContext()) {
    if (source_3psp_key == target_3psp_key) {
      ukm::builders::PostMessage_Incoming_FirstPartyToFirstParty_SameBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    } else {
      ukm::builders::PostMessage_Incoming_FirstPartyToFirstParty_DifferentBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    }
  } else if (source_3psp_key.IsFirstPartyContext() &&
             target_3psp_key.IsThirdPartyContext()) {
    if (source_3psp_key.origin() == target_3psp_key.origin()) {
      ukm::builders::
          PostMessage_Incoming_FirstPartyToThirdParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    } else {
      ukm::builders::
          PostMessage_Incoming_FirstPartyToThirdParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    }
  } else if (source_3psp_key.IsThirdPartyContext() &&
             target_3psp_key.IsFirstPartyContext()) {
    if (source_3psp_key.origin() == target_3psp_key.origin()) {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToFirstParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    } else {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToFirstParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    }
  } else if (source_3psp_key.IsThirdPartyContext() &&
             target_3psp_key.IsThirdPartyContext()) {
    if (source_3psp_key == target_3psp_key) {
      ukm::builders::PostMessage_Incoming_ThirdPartyToThirdParty_SameBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    } else if (source_3psp_key.origin() == target_3psp_key.origin()) {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToThirdParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    } else {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToThirdParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
    }
  }

  return should_send_post_message;
}

}  // namespace blink

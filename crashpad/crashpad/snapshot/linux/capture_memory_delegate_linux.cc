// Copyright 2021 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/linux/capture_memory_delegate_linux.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "snapshot/memory_snapshot_generic.h"

namespace crashpad {
namespace internal {

CaptureMemoryDelegateLinux::CaptureMemoryDelegateLinux(
    ProcessReaderLinux* process_reader,
    const ProcessReaderLinux::Thread* thread_opt,
    std::vector<std::unique_ptr<MemorySnapshotGeneric>>* snapshots,
    uint32_t* budget_remaining)
    : stack_(thread_opt ? thread_opt->stack_region_address : 0,
             thread_opt ? thread_opt->stack_region_size : 0),
      process_reader_(process_reader),
      snapshots_(snapshots),
      budget_remaining_(budget_remaining) {}

bool CaptureMemoryDelegateLinux::Is64Bit() const {
  return process_reader_->Is64Bit();
}

bool CaptureMemoryDelegateLinux::ReadMemory(uint64_t at,
                                            uint64_t num_bytes,
                                            void* into) const {
  return process_reader_->Memory()->Read(
      at, base::checked_cast<size_t>(num_bytes), into);
}

std::vector<CheckedRange<uint64_t>>
CaptureMemoryDelegateLinux::GetReadableRanges(
    const CheckedRange<uint64_t, uint64_t>& range) const {
  return process_reader_->GetMemoryMap()->GetReadableRanges(range);
}

void CaptureMemoryDelegateLinux::AddNewMemorySnapshot(
    const CheckedRange<uint64_t, uint64_t>& range) {
  // Don't bother storing this memory if it points back into the stack.
  if (stack_.ContainsRange(range))
    return;
  if (range.size() == 0)
    return;
  if (!budget_remaining_ || *budget_remaining_ == 0)
    return;
  snapshots_->push_back(std::make_unique<internal::MemorySnapshotGeneric>());
  internal::MemorySnapshotGeneric* snapshot = snapshots_->back().get();
  snapshot->Initialize(process_reader_->Memory(), range.base(), range.size());
  if (!base::IsValueInRangeForNumericType<int64_t>(range.size())) {
    *budget_remaining_ = 0;
  } else {
    int64_t temp = *budget_remaining_;
    temp -= range.size();
    *budget_remaining_ = base::saturated_cast<uint32_t>(temp);
  }
}

}  // namespace internal
}  // namespace crashpad

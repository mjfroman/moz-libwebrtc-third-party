// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_PARENT_EXECUTION_CONTEXT_TASK_RUNNERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_PARENT_EXECUTION_CONTEXT_TASK_RUNNERS_H_

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/task_type_traits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

// Represents a set of task runners of the parent execution context, or default
// task runners for the current thread if no execution context is available.
class CORE_EXPORT ParentExecutionContextTaskRunners final
    : public GarbageCollected<ParentExecutionContextTaskRunners>,
      public ExecutionContextLifecycleObserver {
 public:
  // Returns task runners associated with a given context. This must be called
  // on the context's context thread, that is, the thread where the context was
  // created.
  static ParentExecutionContextTaskRunners* Create(ExecutionContext*);

  // Returns default task runners of the current thread. This can be called from
  // any threads. This must be used only for shared workers, service workers and
  // tests that don't have a parent frame.
  static ParentExecutionContextTaskRunners* Create();

  // ExecutionContext could be nullptr if the worker is not associated with a
  // particular context.
  explicit ParentExecutionContextTaskRunners(ExecutionContext*);

  ParentExecutionContextTaskRunners(const ParentExecutionContextTaskRunners&) =
      delete;
  ParentExecutionContextTaskRunners& operator=(
      const ParentExecutionContextTaskRunners&) = delete;

  // Might return nullptr for unsupported task types. This can be called from
  // any threads.
  scoped_refptr<base::SingleThreadTaskRunner> Get(TaskType)
      LOCKS_EXCLUDED(lock_);

  void Trace(Visitor*) const override;

 private:
  using TaskRunnerHashMap = HashMap<TaskType,
                                    scoped_refptr<base::SingleThreadTaskRunner>,
                                    WTF::IntHash<TaskType>,
                                    TaskTypeTraits>;

  void ContextDestroyed() LOCKS_EXCLUDED(lock_) override;

  base::Lock lock_;
  TaskRunnerHashMap task_runners_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_PARENT_EXECUTION_CONTEXT_TASK_RUNNERS_H_

/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_EVENT_TRACKER_H_

#include <array>
#include <limits>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// This class takes sched events from the trace and processes them to store
// as sched slices.
class EventTracker {
 public:
  explicit EventTracker(TraceProcessorContext*);
  EventTracker(const EventTracker&) = delete;
  EventTracker& operator=(const EventTracker&) = delete;
  virtual ~EventTracker();

  // This method is called when a sched_switch event is seen in the trace.
  virtual void PushSchedSwitch(uint32_t cpu,
                               int64_t timestamp,
                               uint32_t prev_pid,
                               base::StringView prev_comm,
                               int32_t prev_prio,
                               int64_t prev_state,
                               uint32_t next_pid,
                               base::StringView next_comm,
                               int32_t next_prio);

  // This method is called when parsing a sched_switch encoded in the compact
  // format.
  void PushSchedSwitchCompact(uint32_t cpu,
                              int64_t ts,
                              int64_t prev_state,
                              uint32_t next_pid,
                              int32_t next_prio,
                              StringId next_comm_id);

  // This method is called when a counter event is seen in the trace.
  virtual RowId PushCounter(int64_t timestamp,
                            double value,
                            StringId name_id,
                            int64_t ref,
                            RefType ref_type,
                            bool resolve_utid_to_upid = false);

  // This method is called when a counter event is seen in the trace.
  virtual RowId PushCounter(int64_t timestamp,
                            double value,
                            TraceStorage::CounterDefinitions::Id defn_id);

  // This method is called when a instant event is seen in the trace.
  virtual RowId PushInstant(int64_t timestamp,
                            StringId name_id,
                            double value,
                            int64_t ref,
                            RefType ref_type,
                            bool resolve_utid_to_upid = false);

  // Called at the end of trace to flush any events which are pending to the
  // storage.
  void FlushPendingEvents();

 private:
  // Infromation retained from the preceding sched_switch seen on a given cpu.
  struct PendingSchedInfo {
    // The pending scheduling slice that the next event will complete.
    size_t pending_slice_storage_idx = std::numeric_limits<size_t>::max();

    // pid/utid/prio corresponding to the last sched_switch seen on this cpu
    // (its "next_*" fields). There is some duplication with respect to the
    // slices storage, but we don't always have a slice when decoding events in
    // the compact format.
    uint32_t last_pid = std::numeric_limits<uint32_t>::max();
    UniqueTid last_utid = std::numeric_limits<UniqueTid>::max();
    int32_t last_prio = std::numeric_limits<int32_t>::max();
  };

  // Represents a counter event which is currently pending upid resolution.
  struct PendingUpidResolutionCounter {
    uint32_t row = 0;
    StringId name_id = 0;
    UniqueTid utid = 0;
  };

  // Represents a instant event which is currently pending upid resolution.
  struct PendingUpidResolutionInstant {
    uint32_t row = 0;
    UniqueTid utid = 0;
  };

  size_t AddRawEventAndStartSlice(uint32_t cpu,
                                  int64_t ts,
                                  UniqueTid prev_utid,
                                  uint32_t prev_pid,
                                  StringId prev_comm_id,
                                  int32_t prev_prio,
                                  int64_t prev_state,
                                  UniqueTid next_utid,
                                  uint32_t next_pid,
                                  StringId next_comm_id,
                                  int32_t next_prio);

  void ClosePendingSlice(size_t slice_idx, int64_t ts, int64_t prev_state);

  // Infromation retained from the preceding sched_switch seen on a given cpu.
  std::array<PendingSchedInfo, base::kMaxCpus> pending_sched_per_cpu_{};

  // Store the rows in the counters table which need upids resolved.
  std::vector<PendingUpidResolutionCounter> pending_upid_resolution_counter_;

  // Store the rows in the instants table which need upids resolved.
  std::vector<PendingUpidResolutionInstant> pending_upid_resolution_instant_;

  // Timestamp of the previous event. Used to discard events arriving out
  // of order.
  int64_t prev_timestamp_ = 0;

  static constexpr uint8_t kSchedSwitchMaxFieldId = 7;
  std::array<StringId, kSchedSwitchMaxFieldId + 1> sched_switch_field_ids_;
  StringId sched_switch_id_;

  TraceProcessorContext* const context_;
};
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_EVENT_TRACKER_H_

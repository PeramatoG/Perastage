#pragma once

#include <array>
#include <atomic>
#include <cstdlib>
#include <cstdio>

namespace symbols::diagnostics {

// Opt-in process-local measurements. Release builds compile out collection.
enum class Operation {
  Resolution, FilesystemResolution, ArchiveOpen, GeometryInspection,
  Fingerprint, Capture, WorkerSubmission, WorkerCompletion, SvgGeneration,
  Publication, ResourceWrite, IdleWhileWorkerBusy, CoalescedRequest, GuiRequestPost,
  Count
};
inline constexpr std::array<const char *, static_cast<unsigned>(Operation::Count)>
    operationNames = {"resolution", "filesystem_resolution", "archive_open",
      "geometry_inspection", "fingerprint", "capture", "worker_submission",
      "worker_completion", "svg_generation", "publication", "resource_write",
      "idle_while_worker_busy", "coalesced_request", "gui_request_post"};

#ifndef NDEBUG
struct OperationCounters {
  std::array<std::atomic<unsigned long long>, operationNames.size()> counts{};
  bool enabled = std::getenv("PERASTAGE_SYMBOL_DIAGNOSTICS") != nullptr;
  ~OperationCounters() {
    if (!enabled)
      return;
    std::fputs("fixture_symbol_operations", stderr);
    for (unsigned i = 0; i < counts.size(); ++i)
      std::fprintf(stderr, " %s=%llu", operationNames[i], counts[i].load());
    std::fputc('\n', stderr);
  }
};
inline OperationCounters operationCounters;
inline void Count(Operation operation) {
  if (operationCounters.enabled)
    operationCounters.counts[static_cast<unsigned>(operation)].fetch_add(
        1, std::memory_order_relaxed);
}
#else
inline void Count(Operation) {}
#endif

} // namespace symbols::diagnostics

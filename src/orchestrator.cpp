#include "deepnestcpp/orchestrator.hpp"

#include "deepnestcpp/nfp.hpp"
#include "deepnestcpp/placement.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace deepnest {

namespace {

void precomputeMissingPairs(const std::vector<NfpPair>& pairs, const Config& config, NfpCache& cache, int requestedThreads) {
  if (pairs.empty()) {
    return;
  }

  const size_t workerCount =
      std::min(pairs.size(), static_cast<size_t>(normalizeWorkerCount(requestedThreads)));
  if (workerCount <= 1) {
    for (const auto& pair : pairs) {
      static_cast<void>(getOuterNfp(pair.A, pair.B, false, config, cache));
    }
    return;
  }

  std::atomic<size_t> nextIndex{0};
  std::exception_ptr firstError;
  std::mutex errorMutex;
  std::vector<std::thread> workers;
  workers.reserve(workerCount);

  for (size_t worker = 0; worker < workerCount; ++worker) {
    workers.emplace_back([&]() {
      try {
        while (true) {
          const size_t pairIndex = nextIndex.fetch_add(1);
          if (pairIndex >= pairs.size()) {
            break;
          }
          static_cast<void>(getOuterNfp(pairs[pairIndex].A, pairs[pairIndex].B, false, config, cache));
        }
      } catch (...) {
        std::lock_guard lock(errorMutex);
        if (!firstError) {
          firstError = std::current_exception();
        }
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  if (firstError) {
    std::rethrow_exception(firstError);
  }
}

}  // namespace

BackgroundOrchestrator::BackgroundOrchestrator(NfpCache cache) : cache_(std::move(cache)) {}

PlacementResult BackgroundOrchestrator::run(BackgroundRequest data, EventSink& sink) {
  auto parts = data.individual.placement;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i < data.individual.rotation.size()) {
      parts[i].rotation = data.individual.rotation[i];
    }
    if (i < data.ids.size()) {
      parts[i].id = data.ids[i];
    }
    if (i < data.sources.size()) {
      parts[i].source = data.sources[i];
    }
    if (i < data.filenames.size()) {
      parts[i].filename = data.filenames[i];
    }
    if (!data.config.simplify && i < data.children.size()) {
      parts[i].children = data.children[i];
    }
  }

  auto sheets = data.sheets;
  for (size_t i = 0; i < sheets.size(); ++i) {
    if (i < data.sheetids.size()) {
      sheets[i].id = data.sheetids[i];
    }
    if (i < data.sheetsources.size()) {
      sheets[i].source = data.sheetsources[i];
    }
    if (i < data.sheetchildren.size()) {
      sheets[i].children = data.sheetchildren[i];
    }
  }

  auto pairs = preprocessMissingPairs(parts, cache_);
  sink.onProgress(data.index, 0.0);
  // Only independent cache warm-up runs in parallel; greedy placement stays sequential so
  // accepted placements and tie-breaking remain deterministic for the same input/order.
  precomputeMissingPairs(pairs, data.config, cache_, data.config.threads);
  sink.onProgress(data.index, 0.5);

  sink.onTestStart(sheets, parts, data.config, data.index);
  auto result = placeParts(sheets, parts, data.config, cache_, [&](double p) { sink.onProgress(data.index, p); });
  sink.onResult(result);
  return result;
}

}  // namespace deepnest

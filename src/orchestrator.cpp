#include "deepnestcpp/orchestrator.hpp"

#include "deepnestcpp/nfp.hpp"
#include "deepnestcpp/placement.hpp"

#include <algorithm>

namespace deepnest {

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
  for (size_t i = 0; i < pairs.size(); ++i) {
    sink.onProgress(data.index, 0.5 * (static_cast<double>(i) / std::max<size_t>(1, pairs.size())));
    auto nfp = getOuterNfp(pairs[i].A, pairs[i].B, false, data.config, cache_);
    if (!nfp.has_value()) {
      continue;
    }
  }

  sink.onTestStart(sheets, parts, data.config, data.index);
  auto result = placeParts(sheets, parts, data.config, cache_, [&](double p) { sink.onProgress(data.index, p); });
  sink.onResult(result);
  return result;
}

}  // namespace deepnest

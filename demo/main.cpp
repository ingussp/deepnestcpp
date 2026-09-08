#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/dxf_export.hpp"
#include "deepnestcpp/orchestrator.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace deepnest;

namespace {

int parsePositiveCount(const std::string& value) {
  try {
    const int count = std::stoi(value);
    if (count <= 0) {
      throw std::invalid_argument("non-positive");
    }
    return count;
  } catch (const std::exception&) {
    throw std::invalid_argument("--count must be a positive integer");
  }
}

size_t countPlacedParts(const PlacementResult& result) {
  size_t placed = 0;
  for (const auto& sheet : result.placements) {
    placed += sheet.sheetplacements.size();
  }
  return placed;
}

class StdoutSink : public EventSink {
 public:
  void onTestStart(const std::vector<Polygon>& sheets,
                   const std::vector<Polygon>& parts,
                   const Config&,
                   int index) override {
    std::cout << "start index=" << index << " sheets=" << sheets.size() << " parts=" << parts.size() << "\n";
  }

  void onProgress(int index, double progress) override {
    std::cout << "progress index=" << index << " value=" << progress << "\n";
  }

  void onResult(const PlacementResult& result) override {
    std::cout << "result fitness=" << result.fitness << " placements=" << result.placements.size()
              << " utilisation=" << result.utilisation << "%\n";
  }
};

}  // namespace

int main(int argc, char** argv) {
  std::optional<std::filesystem::path> outputPath;
  int count = kDefaultDemoPartCount;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "Usage: deepnestcpp_demo [--count <N>] [--output <path>] [--help]\n"
                << "  --count <N>   Number of identical star parts to generate (default "
                << kDefaultDemoPartCount << ")\n"
                << "  --output <path> Export placed parts and the 2000x2800 mm sheet to DXF\n";
      return 0;
    }
    if (arg == "--count") {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for --count\n";
        return 1;
      }
      try {
        count = parsePositiveCount(argv[++i]);
      } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
      }
      continue;
    }
    if (arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for --output\n";
        return 1;
      }
      if (outputPath.has_value()) {
        std::cerr << "--output provided more than once\n";
        return 1;
      }
      outputPath = std::filesystem::path(argv[++i]);
      continue;
    }

    std::cerr << "Unknown argument: " << arg << "\n";
    return 1;
  }

  BackgroundRequest req;
  req.index = 1;
  req.config.placementType = "box";
  req.config.rotations = 4;
  req.individual.placement = makeDemoStarParts(count);
  req.individual.rotation.assign(req.individual.placement.size(), 0.0);
  req.ids.reserve(req.individual.placement.size());
  req.sources.reserve(req.individual.placement.size());
  req.children.resize(req.individual.placement.size());
  req.filenames.reserve(req.individual.placement.size());
  for (const auto& part : req.individual.placement) {
    req.ids.push_back(part.id);
    req.sources.push_back(part.source);
    req.filenames.push_back(part.filename);
  }

  req.sheets = {makeDemoSheet()};
  req.sheetids = {req.sheets.front().id};
  req.sheetsources = {req.sheets.front().source};
  req.sheetchildren = {{}};

  auto sourceParts = req.individual.placement;
  for (size_t i = 0; i < sourceParts.size(); ++i) {
    if (i < req.individual.rotation.size()) {
      sourceParts[i].rotation = req.individual.rotation[i];
    }
    if (i < req.ids.size()) {
      sourceParts[i].id = req.ids[i];
    }
    if (i < req.sources.size()) {
      sourceParts[i].source = req.sources[i];
    }
    if (i < req.filenames.size()) {
      sourceParts[i].filename = req.filenames[i];
    }
    if (!req.config.simplify && i < req.children.size()) {
      sourceParts[i].children = req.children[i];
    }
  }

  auto sourceSheets = req.sheets;
  for (size_t i = 0; i < sourceSheets.size(); ++i) {
    if (i < req.sheetids.size()) {
      sourceSheets[i].id = req.sheetids[i];
    }
    if (i < req.sheetsources.size()) {
      sourceSheets[i].source = req.sheetsources[i];
    }
    if (i < req.sheetchildren.size()) {
      sourceSheets[i].children = req.sheetchildren[i];
    }
  }

  StdoutSink sink;
  BackgroundOrchestrator orchestrator;
  auto result = orchestrator.run(req, sink);

  if (outputPath.has_value()) {
    try {
      exportPlacementResultToDxf(*outputPath, sourceSheets, sourceParts, result);
      std::cout << "DXF exported to: " << std::filesystem::absolute(*outputPath).string() << "\n";
    } catch (const std::exception& ex) {
      std::cerr << "Failed to export DXF: " << ex.what() << "\n";
      return 1;
    }
  }

  const size_t placedCount = countPlacedParts(result);
  std::cout << "placed parts: " << placedCount << "\n";
  std::cout << "unplaced parts: " << result.unplaced.size() << "\n";
  std::cout << "placed sheets: " << result.placements.size() << "\n";
  return result.placements.empty() ? 1 : 0;
}

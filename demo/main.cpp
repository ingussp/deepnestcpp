#include "deepnestcpp/dxf_export.hpp"
#include "deepnestcpp/orchestrator.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

using namespace deepnest;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src = "") {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  return p;
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
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "Usage: deepnestcpp_demo [--output <path>] [--help]\n";
      return 0;
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

  Polygon partA = rect(0, 0, 3, 3, "partA");
  Polygon partB = rect(0, 0, 2, 2, "partB");

  req.individual.placement = {partA, partB};
  req.individual.rotation = {0, 0};
  req.ids = {1, 2};
  req.sources = {"partA", "partB"};
  req.children = {{}, {}};
  req.filenames = {"a.svg", "b.svg"};

  req.sheets = {rect(0, 0, 10, 10, "sheet1")};
  req.sheetids = {10};
  req.sheetsources = {"sheet1"};
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

  std::cout << "final placed sheets: " << result.placements.size() << "\n";
  return result.placements.empty() ? 1 : 0;
}

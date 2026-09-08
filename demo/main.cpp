#include "deepnestcpp/orchestrator.hpp"

#include <iostream>

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

int main() {
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

  StdoutSink sink;
  BackgroundOrchestrator orchestrator;
  auto result = orchestrator.run(req, sink);

  std::cout << "final placed sheets: " << result.placements.size() << "\n";
  return result.placements.empty() ? 1 : 0;
}

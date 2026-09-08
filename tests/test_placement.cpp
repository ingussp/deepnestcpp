#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/geometry.hpp"
#include "deepnestcpp/orchestrator.hpp"
#include "deepnestcpp/placement.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <unordered_map>

using namespace deepnest;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src = "") {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  return p;
}

std::vector<Polygon> makeParts() {
  Polygon p1 = rect(0, 0, 3, 3, "p1");
  Polygon p2 = rect(0, 0, 2, 2, "p2");
  p1.rotation = 0;
  p2.rotation = 0;
  return {p1, p2};
}

class NullSink : public EventSink {
 public:
  void onTestStart(const std::vector<Polygon>&, const std::vector<Polygon>&, const Config&, int) override {}
  void onProgress(int, double) override {}
  void onResult(const PlacementResult&) override {}
};

std::vector<Polygon> absolutePlacedPolygons(const PlacementResult& result, const std::vector<Polygon>& sourceParts) {
  std::unordered_map<int, Polygon> partById;
  for (const auto& part : sourceParts) {
    if (part.id.has_value()) {
      partById.emplace(*part.id, part);
    }
  }

  std::vector<Polygon> absolute;
  for (const auto& sheet : result.placements) {
    for (const auto& placement : sheet.sheetplacements) {
      REQUIRE(placement.id.has_value());
      const auto it = partById.find(*placement.id);
      REQUIRE(it != partById.end());
      Polygon instance = it->second;
      instance.rotation = placement.rotation;
      absolute.push_back(shiftPolygon(rotatePolygon(instance, placement.rotation), {placement.x, placement.y, true}));
    }
  }
  return absolute;
}

void requirePlacementValidity(const PlacementResult& result, const Polygon& sheet, const std::vector<Polygon>& sourceParts,
                              const Config& cfg) {
  const auto absolute = absolutePlacedPolygons(result, sourceParts);
  REQUIRE_FALSE(absolute.empty());
  for (const auto& polygon : absolute) {
    REQUIRE_FALSE(hasMaterialOutsideSheet(polygon, sheet, cfg));
  }
  for (size_t i = 0; i < absolute.size(); ++i) {
    for (size_t j = i + 1; j < absolute.size(); ++j) {
      REQUIRE_FALSE(hasMaterialOverlap(absolute[i], absolute[j], cfg));
    }
  }
}

}  // namespace

TEST_CASE("placement works across strategies") {
  for (const std::string strategy : {"gravity", "box", "convexhull"}) {
    Config cfg;
    cfg.placementType = strategy;
    cfg.rotations = 4;

    NfpCache cache;
    auto res = placeParts({rect(0, 0, 10, 10, "s1")}, makeParts(), cfg, cache);
    REQUIRE_FALSE(res.placements.empty());
    REQUIRE(res.unplaced.empty());
    REQUIRE(res.area > 0);
  }
}

TEST_CASE("unplaced parts receive penalty") {
  Config cfg;
  cfg.placementType = "box";

  NfpCache cache;
  Polygon tooBig = rect(0, 0, 50, 50, "big");
  tooBig.rotation = 0;
  auto res = placeParts({rect(0, 0, 10, 10, "s1")}, {tooBig}, cfg, cache);
  REQUIRE_FALSE(res.unplaced.empty());
  REQUIRE(res.fitness > 100000000.0);
}

TEST_CASE("identical parts reuse cached pair geometry without overlap") {
  Config cfg;
  cfg.placementType = "box";
  cfg.rotations = 1;

  Polygon repeated = rect(0, 0, 2, 2, "duplicate");
  repeated.filename = "duplicate.svg";
  repeated.geometryKey = "rect:2x2";

  std::vector<Polygon> parts;
  for (int id = 1; id <= 3; ++id) {
    Polygon copy = repeated;
    copy.id = id;
    parts.push_back(copy);
  }

  NfpCache cache;
  auto res = placeParts({rect(0, 0, 8, 4, "sheet")}, parts, cfg, cache);
  REQUIRE(res.unplaced.empty());
  REQUIRE(res.placements.size() == 1);
  REQUIRE(res.placements.front().sheetplacements.size() == parts.size());
  REQUIRE(cache.innerStoreCount() == 1);
  REQUIRE(cache.outerStoreCount() == 1);

  std::vector<Polygon> absolute;
  for (const auto& placement : res.placements.front().sheetplacements) {
    Polygon instance = repeated;
    instance.id = placement.id;
    instance.rotation = placement.rotation;
    absolute.push_back(shiftPolygon(instance, {placement.x, placement.y, true}));
  }

  for (size_t i = 0; i < absolute.size(); ++i) {
    for (size_t j = i + 1; j < absolute.size(); ++j) {
      REQUIRE_FALSE(hasMaterialOverlap(absolute[i], absolute[j], cfg));
    }
  }
}

TEST_CASE("threads 1 and multi-worker produce the same valid placements") {
  Polygon sheet = makeDemoSheet();
  const auto parts = makeDemoStarParts(8);

  BackgroundRequest singleRequest;
  singleRequest.index = 1;
  singleRequest.config.placementType = "box";
  singleRequest.config.rotations = 4;
  singleRequest.config.threads = 1;
  singleRequest.sheets = {sheet};
  singleRequest.individual.placement = parts;
  singleRequest.individual.rotation.assign(parts.size(), 0.0);

  BackgroundRequest parallelRequest = singleRequest;
  parallelRequest.config.threads = 4;

  NullSink sink;
  BackgroundOrchestrator singleOrchestrator;
  BackgroundOrchestrator parallelOrchestrator;

  const auto singleResult = singleOrchestrator.run(singleRequest, sink);
  const auto parallelResult = parallelOrchestrator.run(parallelRequest, sink);

  REQUIRE(singleResult.unplaced.empty());
  REQUIRE(parallelResult.unplaced.empty());
  REQUIRE(singleResult.placements.size() == parallelResult.placements.size());
  REQUIRE(singleResult.placements.front().sheetplacements.size() == parallelResult.placements.front().sheetplacements.size());

  const auto singleAbsolute = absolutePlacedPolygons(singleResult, parts);
  const auto parallelAbsolute = absolutePlacedPolygons(parallelResult, parts);
  REQUIRE(singleAbsolute.size() == parallelAbsolute.size());
  for (size_t i = 0; i < singleAbsolute.size(); ++i) {
    REQUIRE(singleResult.placements.front().sheetplacements[i].id == parallelResult.placements.front().sheetplacements[i].id);
    REQUIRE(singleResult.placements.front().sheetplacements[i].rotation ==
            Catch::Approx(parallelResult.placements.front().sheetplacements[i].rotation));
    REQUIRE(singleResult.placements.front().sheetplacements[i].x ==
            Catch::Approx(parallelResult.placements.front().sheetplacements[i].x).margin(1e-9));
    REQUIRE(singleResult.placements.front().sheetplacements[i].y ==
            Catch::Approx(parallelResult.placements.front().sheetplacements[i].y).margin(1e-9));
  }

  requirePlacementValidity(singleResult, sheet, parts, singleRequest.config);
  requirePlacementValidity(parallelResult, sheet, parts, parallelRequest.config);
}

TEST_CASE("orchestrator exposes timing stats for NFP and bitmap algorithms") {
  Polygon sheet = makeDemoSheet();
  const auto parts = makeDemoStarParts(3);

  BackgroundRequest nfpRequest;
  nfpRequest.index = 1;
  nfpRequest.config.placementType = "box";
  nfpRequest.config.rotations = 1;
  nfpRequest.config.algorithm = NestingAlgorithm::Nfp;
  nfpRequest.sheets = {sheet};
  nfpRequest.individual.placement = parts;
  nfpRequest.individual.rotation.assign(parts.size(), 0.0);

  BackgroundRequest bitmapRequest = nfpRequest;
  bitmapRequest.config.algorithm = NestingAlgorithm::Bitmap;
  bitmapRequest.config.bitmapResolutionMm = 1.0;
  bitmapRequest.config.bitmapPreferAvx2 = false;

  NullSink sink;
  BackgroundOrchestrator orchestrator;
  const auto nfpStats = orchestrator.runWithStats(nfpRequest, sink);
  const auto bitmapStats = orchestrator.runWithStats(bitmapRequest, sink);

  REQUIRE(nfpStats.timings.totalMs >= 0.0);
  REQUIRE(nfpStats.timings.nfpPrecomputeMs >= 0.0);
  REQUIRE(bitmapStats.timings.totalMs >= 0.0);
  REQUIRE(bitmapStats.timings.bitmapMs >= 0.0);
  REQUIRE(bitmapStats.simdBackend == "scalar");
  REQUIRE_FALSE(nfpStats.placement.placements.empty());
  REQUIRE_FALSE(bitmapStats.placement.placements.empty());
}

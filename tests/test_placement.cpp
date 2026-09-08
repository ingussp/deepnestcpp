#include "deepnestcpp/geometry.hpp"
#include "deepnestcpp/placement.hpp"

#include <catch2/catch_test_macros.hpp>

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

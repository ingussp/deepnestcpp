#include "deepnestcpp/bitmap_nesting.hpp"
#include "deepnestcpp/dxf_export.hpp"
#include "deepnestcpp/geometry.hpp"
#include "deepnestcpp/placement.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace deepnest;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src, int id) {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  p.id = id;
  p.geometryKey = src;
  return p;
}

std::filesystem::path tempDxfPath(const std::string& stem) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / (stem + "_" + std::to_string(stamp) + ".dxf");
}

}  // namespace

TEST_CASE("bitmap scalar placement works on small fixture") {
  Config cfg;
  cfg.algorithm = NestingAlgorithm::Bitmap;
  cfg.bitmapResolutionMm = 1.0;
  cfg.bitmapPreferAvx2 = false;
  cfg.rotations = 1;

  Polygon sheet = rect(0, 0, 12, 6, "sheet", 100);
  auto parts = std::vector<Polygon>{rect(0, 0, 3, 3, "partA", 1), rect(0, 0, 2, 2, "partB", 2)};
  BitmapNestingStats stats;
  const auto result = placePartsBitmap({sheet}, parts, cfg, &stats);

  REQUIRE(result.unplaced.empty());
  REQUIRE(result.placements.size() == 1);
  REQUIRE(result.placements.front().sheetplacements.size() == parts.size());
  REQUIRE(stats.simdBackend == "scalar");
}

TEST_CASE("bitmap placements are inside sheet and non-overlapping") {
  Config cfg;
  cfg.algorithm = NestingAlgorithm::Bitmap;
  cfg.bitmapResolutionMm = 1.0;
  cfg.bitmapPreferAvx2 = false;
  cfg.rotations = 1;

  Polygon sheet = rect(0, 0, 14, 8, "sheet", 100);
  auto parts = std::vector<Polygon>{rect(0, 0, 4, 3, "p1", 1), rect(0, 0, 4, 3, "p2", 2), rect(0, 0, 3, 2, "p3", 3)};
  const auto result = placePartsBitmap({sheet}, parts, cfg);
  REQUIRE(result.unplaced.empty());

  std::vector<Polygon> absolute;
  for (const auto& placement : result.placements.front().sheetplacements) {
    Polygon source = parts[static_cast<size_t>(*placement.id - 1)];
    source.rotation = placement.rotation;
    Polygon placed = shiftPolygon(rotatePolygon(source, placement.rotation), {placement.x, placement.y, true});
    absolute.push_back(std::move(placed));
  }

  for (const auto& poly : absolute) {
    REQUIRE_FALSE(hasMaterialOutsideSheet(poly, sheet, cfg));
  }
  for (size_t i = 0; i < absolute.size(); ++i) {
    for (size_t j = i + 1; j < absolute.size(); ++j) {
      REQUIRE_FALSE(hasMaterialOverlap(absolute[i], absolute[j], cfg));
    }
  }
}

TEST_CASE("bitmap reuses identical masks and returns valid placements") {
  Config cfg;
  cfg.algorithm = NestingAlgorithm::Bitmap;
  cfg.bitmapResolutionMm = 1.0;
  cfg.bitmapPreferAvx2 = false;
  cfg.rotations = 1;

  Polygon sheet = rect(0, 0, 20, 20, "sheet", 100);
  Polygon repeated = rect(0, 0, 3, 3, "same", 1);
  repeated.geometryKey = "same-3x3";
  std::vector<Polygon> parts;
  for (int id = 1; id <= 4; ++id) {
    Polygon copy = repeated;
    copy.id = id;
    parts.push_back(copy);
  }

  BitmapNestingStats stats;
  const auto result = placePartsBitmap({sheet}, parts, cfg, &stats);
  REQUIRE_FALSE(result.placements.empty());
  REQUIRE_FALSE(result.placements.front().sheetplacements.empty());
  REQUIRE(stats.cachedMaskCount == 1);
}

TEST_CASE("bitmap scalar and AVX2 paths are equivalent when AVX2 is supported") {
  if (!bitmapAvx2Supported()) {
    SUCCEED("AVX2 backend not supported in this build/runtime");
    return;
  }

  Config scalarCfg;
  scalarCfg.algorithm = NestingAlgorithm::Bitmap;
  scalarCfg.bitmapResolutionMm = 1.0;
  scalarCfg.bitmapPreferAvx2 = false;
  scalarCfg.rotations = 1;

  Config avxCfg = scalarCfg;
  avxCfg.bitmapPreferAvx2 = true;

  Polygon sheet = rect(0, 0, 20, 10, "sheet", 100);
  auto parts = std::vector<Polygon>{rect(0, 0, 4, 4, "a", 1), rect(0, 0, 3, 3, "b", 2), rect(0, 0, 2, 2, "c", 3)};
  BitmapNestingStats scalarStats;
  BitmapNestingStats avxStats;
  const auto scalarResult = placePartsBitmap({sheet}, parts, scalarCfg, &scalarStats);
  const auto avxResult = placePartsBitmap({sheet}, parts, avxCfg, &avxStats);

  REQUIRE(scalarStats.simdBackend == "scalar");
  REQUIRE(avxStats.simdBackend == "avx2");
  REQUIRE(scalarResult.unplaced.size() == avxResult.unplaced.size());
  REQUIRE(scalarResult.placements.size() == avxResult.placements.size());
  REQUIRE(scalarResult.placements.front().sheetplacements.size() == avxResult.placements.front().sheetplacements.size());
  for (size_t i = 0; i < scalarResult.placements.front().sheetplacements.size(); ++i) {
    const auto& s = scalarResult.placements.front().sheetplacements[i];
    const auto& a = avxResult.placements.front().sheetplacements[i];
    REQUIRE(s.id == a.id);
    REQUIRE(s.rotation == a.rotation);
    REQUIRE(s.x == a.x);
    REQUIRE(s.y == a.y);
  }
}

TEST_CASE("NFP and bitmap placement results are DXF-export compatible") {
  Polygon sheet = rect(0, 0, 20, 20, "sheet", 100);
  auto parts = std::vector<Polygon>{rect(0, 0, 5, 5, "p1", 1), rect(0, 0, 4, 4, "p2", 2)};

  Config nfpCfg;
  nfpCfg.algorithm = NestingAlgorithm::Nfp;
  nfpCfg.placementType = "box";
  nfpCfg.rotations = 1;
  NfpCache cache;
  const auto nfpResult = placeParts({sheet}, parts, nfpCfg, cache);

  Config bitmapCfg = nfpCfg;
  bitmapCfg.algorithm = NestingAlgorithm::Bitmap;
  bitmapCfg.bitmapResolutionMm = 1.0;
  bitmapCfg.bitmapPreferAvx2 = false;
  const auto bitmapResult = placePartsBitmap({sheet}, parts, bitmapCfg);

  const auto nfpPath = tempDxfPath("nfp_compat");
  const auto bmpPath = tempDxfPath("bitmap_compat");
  exportPlacementResultToDxf(nfpPath, {sheet}, parts, nfpResult);
  exportPlacementResultToDxf(bmpPath, {sheet}, parts, bitmapResult);
  REQUIRE(std::filesystem::exists(nfpPath));
  REQUIRE(std::filesystem::exists(bmpPath));
}

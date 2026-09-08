#include "deepnestcpp/dxf_export.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace deepnest;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src = "", std::optional<int> id = std::nullopt) {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  p.id = id;
  return p;
}

std::filesystem::path tempDxfPath(const std::string& stem) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / (stem + "_" + std::to_string(stamp) + ".dxf");
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

}  // namespace

TEST_CASE("DXF export writes expected layers and entities") {
  Polygon sheet = rect(0, 0, 10, 10, "sheet1", 10);
  sheet.children.push_back(rect(2, 2, 2, 2));

  Polygon part = rect(0, 0, 2, 1, "part1", 1);
  part.children.push_back(rect(0.5, 0.25, 0.5, 0.5));

  PlacementResult result;
  SheetPlacement placedSheet;
  placedSheet.sheet = "sheet1";
  placedSheet.sheetid = 10;
  placedSheet.sheetplacements = {Placement{1.0, 2.0, 1, 0.0, "part1"}};
  result.placements = {placedSheet};

  const auto path = tempDxfPath("deepnestcpp_export_markers");
  exportPlacementResultToDxf(path, {sheet}, {part}, result);

  REQUIRE(std::filesystem::exists(path));
  REQUIRE(std::filesystem::file_size(path) > 0);

  const auto dxf = readFile(path);
  REQUIRE(dxf.find("SECTION") != std::string::npos);
  REQUIRE(dxf.find("ENTITIES") != std::string::npos);
  REQUIRE(dxf.find("LWPOLYLINE") != std::string::npos);
  REQUIRE(dxf.find("SHEETS") != std::string::npos);
  REQUIRE(dxf.find("PARTS") != std::string::npos);
  REQUIRE(dxf.find("HOLES") != std::string::npos);
  REQUIRE(dxf.find("EOF") != std::string::npos);
}

TEST_CASE("DXF export applies part rotation and translation") {
  Polygon sheet = rect(0, 0, 20, 20, "sheet1", 10);
  Polygon part = rect(0, 0, 2, 1, "duplicate", 1);
  Polygon part2 = rect(0, 0, 1, 1, "duplicate", 2);

  PlacementResult result;
  SheetPlacement placedSheet;
  placedSheet.sheet = "sheet1";
  placedSheet.sheetid = 10;
  placedSheet.sheetplacements = {Placement{10.0, 5.0, 1, 90.0, "duplicate"}};
  result.placements = {placedSheet};

  const auto path = tempDxfPath("deepnestcpp_export_transform");
  exportPlacementResultToDxf(path, {sheet}, {part, part2}, result);

  const auto dxf = readFile(path);
  REQUIRE(dxf.find("10\n10.000000\n20\n5.000000\n") != std::string::npos);
  REQUIRE(dxf.find("10\n10.000000\n20\n7.000000\n") != std::string::npos);
  REQUIRE(dxf.find("10\n9.000000\n20\n7.000000\n") != std::string::npos);
  REQUIRE(dxf.find("10\n9.000000\n20\n5.000000\n") != std::string::npos);
}

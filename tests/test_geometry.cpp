#include "deepnestcpp/geometry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace deepnest;
using Catch::Approx;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src = "") {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  return p;
}

}  // namespace

TEST_CASE("rotate and shift polygon") {
  Polygon p = rect(0, 0, 2, 1);
  auto r = rotatePolygon(p, 90);
  REQUIRE(r.points.size() == 4);
  REQUIRE(r.points[1].x == Approx(0.0).margin(1e-9));
  REQUIRE(r.points[1].y == Approx(2.0).margin(1e-9));

  auto s = shiftPolygon(p, {3, 4, true});
  REQUIRE(s.points[0].x == Approx(3));
  REQUIRE(s.points[0].y == Approx(4));
}

TEST_CASE("material area subtracts holes") {
  Polygon p = rect(0, 0, 10, 10);
  p.children.push_back(rect(2, 2, 3, 3));
  REQUIRE(polygonMaterialArea(p) == Approx(91.0));
}

TEST_CASE("merged edge detection") {
  Polygon base = rect(0, 0, 10, 10);
  Polygon other = rect(10, 0, 5, 10);

  auto merged = mergedLength({base}, other, 0.5, 1e-6);
  REQUIRE(merged.totalLength >= Approx(10.0).epsilon(1e-6));
  REQUIRE_FALSE(merged.segments.empty());
}

TEST_CASE("overlap and outside-sheet checks") {
  Config cfg;
  Polygon a = rect(0, 0, 10, 10);
  Polygon b = rect(5, 5, 10, 10);
  Polygon c = rect(20, 20, 2, 2);

  REQUIRE(hasMaterialOverlap(a, b, cfg));
  REQUIRE_FALSE(hasMaterialOverlap(a, c, cfg));

  Polygon sheet = rect(0, 0, 20, 20);
  Polygon inside = rect(1, 1, 2, 2);
  Polygon outside = rect(19, 19, 3, 3);
  REQUIRE_FALSE(hasMaterialOutsideSheet(inside, sheet, cfg));
  REQUIRE(hasMaterialOutsideSheet(outside, sheet, cfg));
}

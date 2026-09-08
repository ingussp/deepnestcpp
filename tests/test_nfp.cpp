#include "deepnestcpp/nfp.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace deepnest;

namespace {

Polygon rect(double x, double y, double w, double h, const std::string& src = "") {
  Polygon p;
  p.points = {{x, y, true}, {x + w, y, true}, {x + w, y + h, true}, {x, y + h, true}};
  p.source = src;
  return p;
}

}  // namespace

TEST_CASE("outer NFP for two rectangles") {
  Config cfg;
  NfpCache cache;
  Polygon A = rect(0, 0, 10, 10, "A");
  Polygon B = rect(0, 0, 3, 3, "B");

  auto nfp = getOuterNfp(A, B, false, cfg, cache);
  REQUIRE(nfp.has_value());
  REQUIRE(nfp->points.size() >= 4);
}

TEST_CASE("inner NFP exists for fit and hole-aware material") {
  Config cfg;
  NfpCache cache;
  Polygon sheet = rect(0, 0, 10, 10, "S");
  sheet.children.push_back(rect(4, 4, 2, 2, "H"));
  Polygon part = rect(0, 0, 2, 2, "P");

  auto inner = getInnerNfp(sheet, part, cfg, cache);
  REQUIRE(inner.has_value());
  REQUIRE_FALSE(inner->empty());
}

TEST_CASE("preprocessMissingPairs collapses identical geometry copies") {
  NfpCache cache;

  Polygon repeated = rect(0, 0, 2, 2, "duplicate");
  repeated.geometryKey = "rect:2x2";
  repeated.rotation = 0.0;

  std::vector<Polygon> parts(4, repeated);
  auto pairs = preprocessMissingPairs(parts, cache);

  REQUIRE(pairs.size() == 1);
  REQUIRE(pairs.front().A.source == "duplicate");
  REQUIRE(pairs.front().B.source == "duplicate");
}

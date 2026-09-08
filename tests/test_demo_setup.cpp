#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/geometry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace deepnest;
using Catch::Approx;

TEST_CASE("demo star parts share geometry identity and unique ids") {
  const auto parts = makeDemoStarParts(5);

  REQUIRE(parts.size() == 5);
  const std::string identity = polygonGeometryIdentity(parts.front());
  std::set<int> ids;
  for (const auto& part : parts) {
    REQUIRE(part.id.has_value());
    ids.insert(*part.id);
    REQUIRE(part.source == "neregulara_zvaigzne");
    REQUIRE(part.filename == "neregulara_zvaigzne.svg");
    REQUIRE(polygonGeometryIdentity(part) == identity);
  }
  REQUIRE(ids.size() == parts.size());
}

TEST_CASE("demo geometry uses normalized transformed star and full sheet size") {
  const Polygon star = makeDemoStarPolygon();
  const Polygon sheet = makeDemoSheet();
  const Bounds starBounds = getPolygonBounds(star.points);
  const Bounds sheetBounds = getPolygonBounds(sheet.points);

  REQUIRE(star.points.size() == 8);
  REQUIRE(star.points.front().x == Approx(0.0));
  REQUIRE(star.points.front().y == Approx(0.0));
  REQUIRE(star.points[1].y == Approx(-25.039257));
  REQUIRE(starBounds.width == Approx(120.447716).margin(1e-6));
  REQUIRE(starBounds.height == Approx(113.308991).margin(1e-6));
  REQUIRE(sheetBounds.width == Approx(kDemoSheetWidthMm));
  REQUIRE(sheetBounds.height == Approx(kDemoSheetHeightMm));
}

#include "deepnestcpp/demo_cli.hpp"
#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/model.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

using namespace deepnest;

TEST_CASE("demo CLI parses explicit thread count and output") {
  const auto options = parseDemoCliOptions(
      {"--count", "20", "--threads", "8", "--algorithm", "bitmap", "--bitmap-resolution", "0.5", "--output", "result.dxf"});

  REQUIRE(options.count == 20);
  REQUIRE(options.threads == 8);
  REQUIRE(options.algorithm == NestingAlgorithm::Bitmap);
  REQUIRE(options.bitmapResolutionMm == Catch::Approx(0.5));
  REQUIRE(options.outputPath.has_value());
  REQUIRE(options.outputPath->string() == "result.dxf");
  REQUIRE_FALSE(options.showHelp);
}

TEST_CASE("demo CLI defaults thread count from hardware with fallback") {
  const auto options = parseDemoCliOptions({});

  REQUIRE(options.count == kDefaultDemoPartCount);
  REQUIRE(options.threads == defaultWorkerCount());
  REQUIRE(options.algorithm == NestingAlgorithm::Nfp);
  REQUIRE(options.bitmapResolutionMm == Catch::Approx(1.0));
  REQUIRE(options.threads >= 1);
}

TEST_CASE("demo CLI rejects non-positive thread count") {
  try {
    static_cast<void>(parseDemoCliOptions({"--threads", "0"}));
    FAIL("expected parseDemoCliOptions to reject --threads 0");
  } catch (const std::invalid_argument& ex) {
    REQUIRE(std::string(ex.what()) == "--threads must be a positive integer");
  }
}

TEST_CASE("demo CLI rejects invalid algorithm and bitmap resolution") {
  REQUIRE_THROWS_WITH(parseDemoCliOptions({"--algorithm", "foo"}),
                      Catch::Matchers::ContainsSubstring("Unknown algorithm"));
  REQUIRE_THROWS_WITH(parseDemoCliOptions({"--bitmap-resolution", "0"}),
                      Catch::Matchers::ContainsSubstring("--bitmap-resolution must be a positive number"));
}

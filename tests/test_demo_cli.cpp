#include "deepnestcpp/demo_cli.hpp"
#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/model.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

using namespace deepnest;

TEST_CASE("demo CLI parses explicit thread count and output") {
  const auto options = parseDemoCliOptions({"--count", "20", "--threads", "8", "--output", "result.dxf"});

  REQUIRE(options.count == 20);
  REQUIRE(options.threads == 8);
  REQUIRE(options.outputPath.has_value());
  REQUIRE(options.outputPath->string() == "result.dxf");
  REQUIRE_FALSE(options.showHelp);
}

TEST_CASE("demo CLI defaults thread count from hardware with fallback") {
  const auto options = parseDemoCliOptions({});

  REQUIRE(options.count == kDefaultDemoPartCount);
  REQUIRE(options.threads == defaultWorkerCount());
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

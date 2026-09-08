#include "deepnestcpp/demo_cli.hpp"

#include "deepnestcpp/demo_setup.hpp"
#include "deepnestcpp/model.hpp"

#include <exception>
#include <stdexcept>
#include <string>

namespace deepnest {

namespace {

int parsePositiveValue(std::string_view raw, const char* flagName) {
  try {
    const int value = std::stoi(std::string(raw));
    if (value <= 0) {
      throw std::invalid_argument("non-positive");
    }
    return value;
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string(flagName) + " must be a positive integer");
  }
}

}  // namespace

DemoCliOptions parseDemoCliOptions(const std::vector<std::string_view>& args) {
  DemoCliOptions options{kDefaultDemoPartCount, defaultWorkerCount(), std::nullopt, false};

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "--help") {
      options.showHelp = true;
      return options;
    }
    if (arg == "--count") {
      if (i + 1 >= args.size()) {
        throw std::invalid_argument("Missing value for --count");
      }
      options.count = parsePositiveValue(args[++i], "--count");
      continue;
    }
    if (arg == "--threads") {
      if (i + 1 >= args.size()) {
        throw std::invalid_argument("Missing value for --threads");
      }
      options.threads = parsePositiveValue(args[++i], "--threads");
      continue;
    }
    if (arg == "--output") {
      if (i + 1 >= args.size()) {
        throw std::invalid_argument("Missing value for --output");
      }
      if (options.outputPath.has_value()) {
        throw std::invalid_argument("--output provided more than once");
      }
      options.outputPath = std::filesystem::path(args[++i]);
      continue;
    }

    throw std::invalid_argument("Unknown argument: " + std::string(arg));
  }

  options.threads = normalizeWorkerCount(options.threads);
  return options;
}

}  // namespace deepnest

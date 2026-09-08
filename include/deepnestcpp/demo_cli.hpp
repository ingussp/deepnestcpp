#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace deepnest {

struct DemoCliOptions {
  int count;
  int threads;
  std::optional<std::filesystem::path> outputPath;
  bool showHelp{false};
};

DemoCliOptions parseDemoCliOptions(const std::vector<std::string_view>& args);

}  // namespace deepnest

#pragma once

#include "deepnestcpp/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace deepnest {

struct BitmapNestingStats {
  std::string simdBackend{"scalar"};
  size_t cachedMaskCount{0};
};

PlacementResult placePartsBitmap(const std::vector<Polygon>& sheets,
                                 const std::vector<Polygon>& parts,
                                 const Config& config,
                                 BitmapNestingStats* stats = nullptr);

bool bitmapAvx2Supported();

}  // namespace deepnest

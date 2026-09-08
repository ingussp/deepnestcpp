#pragma once

#include "deepnestcpp/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace deepnest {

struct BitmapNestingStats {
  std::string simdBackend{"scalar"};
  size_t cachedMaskCount{0};
  size_t processedParts{0};
  size_t placedParts{0};
  size_t unplacedParts{0};
  size_t candidatesExamined{0};
  size_t boundaryRejects{0};
  size_t bitmapCollisionRejects{0};
  size_t vectorValidationRejects{0};
  struct PartStats {
    size_t processedPart{0};
    bool placed{false};
    double elapsedMs{0.0};
    size_t candidatesExamined{0};
    size_t boundaryRejects{0};
    size_t bitmapCollisionRejects{0};
    size_t vectorValidationRejects{0};
  };
  std::vector<PartStats> perPart;
};

PlacementResult placePartsBitmap(const std::vector<Polygon>& sheets,
                                 const std::vector<Polygon>& parts,
                                 const Config& config,
                                 BitmapNestingStats* stats = nullptr);

bool bitmapAvx2Supported();

}  // namespace deepnest

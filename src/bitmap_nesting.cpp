#include "deepnestcpp/bitmap_nesting.hpp"

#include "deepnestcpp/geometry.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__AVX2__) || (defined(__GNUC__) && !defined(_MSC_VER) && (defined(__x86_64__) || defined(__i386__)))
#include <immintrin.h>
#endif

namespace deepnest {

namespace {

constexpr size_t kMaxBitmapPixels = 200000000ULL;

struct BitmapGrid {
  int widthPx{0};
  int heightPx{0};
  size_t wordsPerRow{0};
  std::vector<uint64_t> bits;
};

struct RasterMask {
  Polygon rotatedPart;
  double rotationDeg{0.0};
  Bounds rotatedBounds;
  double minX{0.0};
  double minY{0.0};
  int widthPx{0};
  int heightPx{0};
  size_t wordsPerRow{0};
  std::vector<uint64_t> bits;
};

bool boundsIntersect(const Bounds& a, const Bounds& b) {
  const double aRight = a.x + a.width;
  const double aBottom = a.y + a.height;
  const double bRight = b.x + b.width;
  const double bBottom = b.y + b.height;
  return !(aRight <= b.x || bRight <= a.x || aBottom <= b.y || bBottom <= a.y);
}

bool pointInSimplePolygon(const std::vector<Point>& polygon, const Point& p) {
  if (polygon.size() < 3) {
    return false;
  }
  bool inside = false;
  for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const auto& a = polygon[i];
    const auto& b = polygon[j];
    const bool intersects = ((a.y > p.y) != (b.y > p.y)) &&
                            (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) == 0.0 ? 1e-12 : (b.y - a.y)) + a.x);
    if (intersects) {
      inside = !inside;
    }
  }
  return inside;
}

bool pointInPolygonMaterial(const Polygon& polygon, const Point& p) {
  if (!pointInSimplePolygon(polygon.points, p)) {
    return false;
  }
  for (const auto& hole : polygon.children) {
    if (pointInSimplePolygon(hole.points, p)) {
      return false;
    }
  }
  return true;
}

void ensureBitmapFitsMemory(int widthPx, int heightPx, const char* what) {
  if (widthPx <= 0 || heightPx <= 0) {
    throw std::invalid_argument(std::string(what) + " raster dimensions must be positive");
  }

  const size_t pixels = static_cast<size_t>(widthPx) * static_cast<size_t>(heightPx);
  if (pixels > kMaxBitmapPixels) {
    throw std::invalid_argument(std::string(what) + " raster is too large; increase --bitmap-resolution");
  }
}

BitmapGrid makeBitmapGrid(int widthPx, int heightPx) {
  ensureBitmapFitsMemory(widthPx, heightPx, "Bitmap");
  BitmapGrid grid;
  grid.widthPx = widthPx;
  grid.heightPx = heightPx;
  grid.wordsPerRow = static_cast<size_t>((widthPx + 63) / 64);
  grid.bits.assign(grid.wordsPerRow * static_cast<size_t>(heightPx), 0ULL);
  return grid;
}

inline uint64_t* rowBits(BitmapGrid& grid, int y) {
  return &grid.bits[static_cast<size_t>(y) * grid.wordsPerRow];
}

inline const uint64_t* rowBits(const BitmapGrid& grid, int y) {
  return &grid.bits[static_cast<size_t>(y) * grid.wordsPerRow];
}

void setPixel(BitmapGrid& grid, int x, int y) {
  if (x < 0 || y < 0 || x >= grid.widthPx || y >= grid.heightPx) {
    return;
  }
  const size_t idx = static_cast<size_t>(y) * grid.wordsPerRow + static_cast<size_t>(x / 64);
  grid.bits[idx] |= (1ULL << static_cast<unsigned>(x % 64));
}

bool avx2RuntimeAvailable() {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#if defined(_MSC_VER)
  int regs[4] = {0, 0, 0, 0};
  __cpuidex(regs, 1, 0);
  const bool osxsave = (regs[2] & (1 << 27)) != 0;
  const bool avx = (regs[2] & (1 << 28)) != 0;
  if (!osxsave || !avx) {
    return false;
  }
  const unsigned long long xcr0 = _xgetbv(0);
  if ((xcr0 & 0x6) != 0x6) {
    return false;
  }
  __cpuidex(regs, 7, 0);
  return (regs[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
#else
  return false;
#endif
}

#if defined(__AVX2__)
bool rowsCollideAndInBoundsAvx2(const uint64_t* occ, const uint64_t* material, const uint64_t* mask, size_t words) {
  size_t i = 0;
  const size_t vecWords = (words / 4) * 4;
  for (; i < vecWords; i += 4) {
    const __m256i occVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(occ + i));
    const __m256i matVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(material + i));
    const __m256i maskVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(mask + i));
    const __m256i overlap = _mm256_and_si256(maskVec, occVec);
    const __m256i outside = _mm256_andnot_si256(matVec, maskVec);
    if (!_mm256_testz_si256(overlap, overlap) || !_mm256_testz_si256(outside, outside)) {
      return true;
    }
  }
  for (; i < words; ++i) {
    const uint64_t overlap = occ[i] & mask[i];
    const uint64_t outside = (~material[i]) & mask[i];
    if ((overlap | outside) != 0ULL) {
      return true;
    }
  }
  return false;
}

void rowsOrAvx2(uint64_t* occ, const uint64_t* mask, size_t words) {
  size_t i = 0;
  const size_t vecWords = (words / 4) * 4;
  for (; i < vecWords; i += 4) {
    const __m256i occVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(occ + i));
    const __m256i maskVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(mask + i));
    const __m256i out = _mm256_or_si256(occVec, maskVec);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(occ + i), out);
  }
  for (; i < words; ++i) {
    occ[i] |= mask[i];
  }
}
#endif

bool rowsCollideAndInBoundsScalar(const uint64_t* occ, const uint64_t* material, const uint64_t* mask, size_t words) {
  for (size_t i = 0; i < words; ++i) {
    const uint64_t overlap = occ[i] & mask[i];
    const uint64_t outside = (~material[i]) & mask[i];
    if ((overlap | outside) != 0ULL) {
      return true;
    }
  }
  return false;
}

void rowsOrScalar(uint64_t* occ, const uint64_t* mask, size_t words) {
  for (size_t i = 0; i < words; ++i) {
    occ[i] |= mask[i];
  }
}

RasterMask rasterizePartMask(const Polygon& part, double rotationDeg, double resolutionMm) {
  RasterMask mask;
  mask.rotationDeg = rotationDeg;
  mask.rotatedPart = rotatePolygon(part, rotationDeg);
  mask.rotatedPart.rotation = rotationDeg;
  mask.rotatedPart.id = part.id;
  mask.rotatedPart.source = part.source;
  mask.rotatedPart.filename = part.filename;

  const Bounds bounds = getPolygonBounds(mask.rotatedPart.points);
  mask.rotatedBounds = bounds;
  mask.minX = bounds.x;
  mask.minY = bounds.y;
  mask.widthPx = std::max(1, static_cast<int>(std::ceil(bounds.width / resolutionMm)));
  mask.heightPx = std::max(1, static_cast<int>(std::ceil(bounds.height / resolutionMm)));
  ensureBitmapFitsMemory(mask.widthPx, mask.heightPx, "Part");

  mask.wordsPerRow = static_cast<size_t>((mask.widthPx + 63) / 64);
  mask.bits.assign(mask.wordsPerRow * static_cast<size_t>(mask.heightPx), 0ULL);

  for (int y = 0; y < mask.heightPx; ++y) {
    const double py = mask.minY + (static_cast<double>(y) + 0.5) * resolutionMm;
    for (int x = 0; x < mask.widthPx; ++x) {
      const double px = mask.minX + (static_cast<double>(x) + 0.5) * resolutionMm;
      if (pointInPolygonMaterial(mask.rotatedPart, {px, py, true})) {
        const size_t idx = static_cast<size_t>(y) * mask.wordsPerRow + static_cast<size_t>(x / 64);
        mask.bits[idx] |= (1ULL << static_cast<unsigned>(x % 64));
      }
    }
  }

  return mask;
}

BitmapGrid rasterizeSheetMaterial(const Polygon& sheet, double resolutionMm, Bounds& sheetBounds) {
  sheetBounds = getPolygonBounds(sheet.points);
  const int widthPx = std::max(1, static_cast<int>(std::ceil(sheetBounds.width / resolutionMm)));
  const int heightPx = std::max(1, static_cast<int>(std::ceil(sheetBounds.height / resolutionMm)));
  BitmapGrid material = makeBitmapGrid(widthPx, heightPx);

  for (int y = 0; y < material.heightPx; ++y) {
    const double py = sheetBounds.y + (static_cast<double>(y) + 0.5) * resolutionMm;
    for (int x = 0; x < material.widthPx; ++x) {
      const double px = sheetBounds.x + (static_cast<double>(x) + 0.5) * resolutionMm;
      if (pointInPolygonMaterial(sheet, {px, py, true})) {
        setPixel(material, x, y);
      }
    }
  }

  return material;
}

uint64_t shiftedMaskWord(const uint64_t* src, size_t srcWords, size_t wordIndex, int bitShift) {
  const uint64_t lo = wordIndex < srcWords ? src[wordIndex] : 0ULL;
  const uint64_t hi = (wordIndex + 1) < srcWords ? src[wordIndex + 1] : 0ULL;
  return (lo << bitShift) | (hi >> (64 - bitShift));
}

bool rowsCollideAndInBoundsShiftedScalar(const uint64_t* occ,
                                         const uint64_t* material,
                                         const uint64_t* maskRow,
                                         size_t maskWords,
                                         int bitShift) {
  const size_t shiftedWords = maskWords + 1;
  for (size_t i = 0; i < shiftedWords; ++i) {
    const uint64_t shiftedWord = shiftedMaskWord(maskRow, maskWords, i, bitShift);
    if ((shiftedWord & occ[i]) != 0ULL || (shiftedWord & ~material[i]) != 0ULL) {
      return true;
    }
  }
  return false;
}

void rowsOrShiftedScalar(uint64_t* occ, const uint64_t* maskRow, size_t maskWords, int bitShift) {
  const size_t shiftedWords = maskWords + 1;
  for (size_t i = 0; i < shiftedWords; ++i) {
    occ[i] |= shiftedMaskWord(maskRow, maskWords, i, bitShift);
  }
}

bool maskFits(const BitmapGrid& occupancy,
              const BitmapGrid& material,
              const RasterMask& mask,
              int originX,
              int originY,
              bool useAvx2) {
  if (originX < 0 || originY < 0) {
    return false;
  }
  if (originX + mask.widthPx > occupancy.widthPx || originY + mask.heightPx > occupancy.heightPx) {
    return false;
  }

  const int wordShift = originX / 64;
  const int bitShift = originX % 64;
  const size_t neededWords = mask.wordsPerRow + (bitShift == 0 ? 0 : 1);
  if (static_cast<size_t>(wordShift) + neededWords > occupancy.wordsPerRow) {
    return false;
  }

  for (int row = 0; row < mask.heightPx; ++row) {
    const uint64_t* occRow = rowBits(occupancy, originY + row) + wordShift;
    const uint64_t* matRow = rowBits(material, originY + row) + wordShift;
    if (bitShift == 0) {
      const uint64_t* maskRow = &mask.bits[static_cast<size_t>(row) * mask.wordsPerRow];
#if defined(__AVX2__)
      if (useAvx2) {
        if (rowsCollideAndInBoundsAvx2(occRow, matRow, maskRow, mask.wordsPerRow)) {
          return false;
        }
      } else
#endif
      {
        if (rowsCollideAndInBoundsScalar(occRow, matRow, maskRow, mask.wordsPerRow)) {
          return false;
        }
      }
    } else {
      const uint64_t* maskRow = &mask.bits[static_cast<size_t>(row) * mask.wordsPerRow];
      if (rowsCollideAndInBoundsShiftedScalar(occRow, matRow, maskRow, mask.wordsPerRow, bitShift)) {
        return false;
      }
    }
  }
  return true;
}

void applyMask(BitmapGrid& occupancy, const RasterMask& mask, int originX, int originY, bool useAvx2) {
  const int wordShift = originX / 64;
  const int bitShift = originX % 64;

  for (int row = 0; row < mask.heightPx; ++row) {
    uint64_t* occRow = rowBits(occupancy, originY + row) + wordShift;
    if (bitShift == 0) {
      const uint64_t* maskRow = &mask.bits[static_cast<size_t>(row) * mask.wordsPerRow];
#if defined(__AVX2__)
      if (useAvx2) {
        rowsOrAvx2(occRow, maskRow, mask.wordsPerRow);
      } else
#endif
      {
        rowsOrScalar(occRow, maskRow, mask.wordsPerRow);
      }
    } else {
      const uint64_t* maskRow = &mask.bits[static_cast<size_t>(row) * mask.wordsPerRow];
      rowsOrShiftedScalar(occRow, maskRow, mask.wordsPerRow, bitShift);
    }
  }
}

std::string bitmapMaskCacheKey(const Polygon& part, double rotation) {
  return polygonGeometryIdentity(part) + "|" + std::to_string(rotation);
}

PlacementResult placePartsBitmapOnSingleSheet(const Polygon& sheet,
                                              const std::vector<Polygon>& parts,
                                              const Config& config,
                                              bool useAvx2,
                                              BitmapNestingStats* stats,
                                              const BitmapPartProgressCallback& onPartProgress) {
  PlacementResult out;
  out.totalarea = polygonMaterialArea(sheet);

  Bounds sheetBounds;
  BitmapGrid material = rasterizeSheetMaterial(sheet, config.bitmapResolutionMm, sheetBounds);
  BitmapGrid occupancy = makeBitmapGrid(material.widthPx, material.heightPx);

  std::unordered_map<std::string, RasterMask> maskCache;
  std::vector<Polygon> placedAbsolute;
  std::vector<Bounds> placedBounds;
  std::vector<Placement> placements;
  size_t placedCount = 0;
  size_t unplacedCount = 0;
  const size_t totalParts = parts.size();
  const int searchStep = std::max(1, config.bitmapSearchStepPx);
  BitmapNestingStats localStats;
  const auto bitmapStart = std::chrono::steady_clock::now();

  for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex) {
    const Polygon& part = parts[partIndex];
    const auto partStart = std::chrono::steady_clock::now();

    const int rotationCount = std::max(1, config.rotations);
    const double rotationStep = 360.0 / static_cast<double>(rotationCount);
    std::vector<const RasterMask*> rotationMasks;
    rotationMasks.reserve(static_cast<size_t>(rotationCount));
    for (int r = 0; r < rotationCount; ++r) {
      const double rotation = part.rotation + rotationStep * static_cast<double>(r);
      const std::string key = bitmapMaskCacheKey(part, rotation);
      auto it = maskCache.find(key);
      if (it == maskCache.end()) {
        it = maskCache.emplace(key, rasterizePartMask(part, rotation, config.bitmapResolutionMm)).first;
      }
      rotationMasks.push_back(&it->second);
    }

    std::optional<Placement> acceptedPlacement;
    const RasterMask* acceptedMask = nullptr;
    int acceptedX = 0;
    int acceptedY = 0;
    size_t partCandidatesExamined = 0;
    size_t partBoundaryRejects = 0;
    size_t partBitmapCollisions = 0;
    size_t partVectorValidationRejects = 0;
    bool placed = false;
    int globalMaxOriginX = std::numeric_limits<int>::min();
    int globalMaxOriginY = std::numeric_limits<int>::min();
    for (const RasterMask* mask : rotationMasks) {
      globalMaxOriginX = std::max(globalMaxOriginX, material.widthPx - mask->widthPx);
      globalMaxOriginY = std::max(globalMaxOriginY, material.heightPx - mask->heightPx);
    }

    Polygon acceptedAbsolute;
    Bounds acceptedBounds;

    for (int x = 0; !placed && x <= globalMaxOriginX; x += searchStep) {
      for (int y = 0; !placed && y <= globalMaxOriginY; y += searchStep) {
        for (const RasterMask* mask : rotationMasks) {
          ++partCandidatesExamined;
          if (x > material.widthPx - mask->widthPx || y > material.heightPx - mask->heightPx) {
            ++partBoundaryRejects;
            continue;
          }
          if (!maskFits(occupancy, material, *mask, x, y, useAvx2)) {
            ++partBitmapCollisions;
            continue;
          }

          const double worldMaskMinX = sheetBounds.x + static_cast<double>(x) * config.bitmapResolutionMm;
          const double worldMaskMinY = sheetBounds.y + static_cast<double>(y) * config.bitmapResolutionMm;
          const double shiftX = worldMaskMinX - mask->minX;
          const double shiftY = worldMaskMinY - mask->minY;

          if (config.bitmapValidateGeometry) {
            Polygon absolute = shiftPolygon(mask->rotatedPart, {shiftX, shiftY, true});
            if (hasMaterialOutsideSheet(absolute, sheet, config)) {
              ++partVectorValidationRejects;
              continue;
            }

            const Bounds absoluteBounds{mask->rotatedBounds.x + shiftX,
                                        mask->rotatedBounds.y + shiftY,
                                        mask->rotatedBounds.width,
                                        mask->rotatedBounds.height};
            bool overlap = false;
            for (size_t placedIndex = 0; placedIndex < placedAbsolute.size(); ++placedIndex) {
              if (!boundsIntersect(absoluteBounds, placedBounds[placedIndex])) {
                continue;
              }
              if (hasMaterialOverlap(absolute, placedAbsolute[placedIndex], config)) {
                overlap = true;
                break;
              }
            }
            if (overlap) {
              ++partVectorValidationRejects;
              continue;
            }
            acceptedAbsolute = std::move(absolute);
            acceptedBounds = absoluteBounds;
          }
          acceptedPlacement = Placement{
              shiftX, shiftY, part.id, mask->rotationDeg, part.source, part.filename, 0.0, {}};
          acceptedMask = mask;
          acceptedX = x;
          acceptedY = y;
          placed = true;
          break;
        }
      }
    }

    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - partStart).count();

    if (!acceptedPlacement.has_value() || acceptedMask == nullptr) {
      out.unplaced.push_back(part);
      ++unplacedCount;
      localStats.unplacedParts++;
      localStats.processedParts++;
      localStats.candidatesExamined += partCandidatesExamined;
      localStats.boundaryRejects += partBoundaryRejects;
      localStats.bitmapCollisions += partBitmapCollisions;
      localStats.vectorValidationRejects += partVectorValidationRejects;
      auto& partStats = localStats.perPart.emplace_back(BitmapNestingStats::PartStats{
          partIndex + 1,
          false,
          placedCount,
          unplacedCount,
          elapsedMs,
          partCandidatesExamined,
          partBoundaryRejects,
          partBitmapCollisions,
          partVectorValidationRejects});
      if (onPartProgress) {
        onPartProgress(partStats, totalParts);
      }
      continue;
    }

    applyMask(occupancy, *acceptedMask, acceptedX, acceptedY, useAvx2);
    placements.push_back(*acceptedPlacement);
    Polygon placedPolygon;
    if (config.bitmapValidateGeometry) {
      placedPolygon = acceptedAbsolute;
      placedBounds.push_back(acceptedBounds);
    } else {
      placedPolygon = shiftPolygon(acceptedMask->rotatedPart, {acceptedPlacement->x, acceptedPlacement->y, true});
      placedBounds.push_back(Bounds{acceptedMask->rotatedBounds.x + acceptedPlacement->x,
                                    acceptedMask->rotatedBounds.y + acceptedPlacement->y,
                                    acceptedMask->rotatedBounds.width,
                                    acceptedMask->rotatedBounds.height});
    }
    placedAbsolute.push_back(placedPolygon);
    out.area += polygonMaterialArea(placedPolygon);
    ++placedCount;
    localStats.acceptedPlacements++;
    localStats.placedParts++;
    localStats.processedParts++;
    localStats.candidatesExamined += partCandidatesExamined;
    localStats.boundaryRejects += partBoundaryRejects;
    localStats.bitmapCollisions += partBitmapCollisions;
    localStats.vectorValidationRejects += partVectorValidationRejects;
    auto& partStats = localStats.perPart.emplace_back(BitmapNestingStats::PartStats{
        partIndex + 1,
        true,
        placedCount,
        unplacedCount,
        elapsedMs,
        partCandidatesExamined,
        partBoundaryRejects,
        partBitmapCollisions,
        partVectorValidationRejects});
    if (onPartProgress) {
      onPartProgress(partStats, totalParts);
    }
  }

  localStats.cachedMaskCount = maskCache.size();
  localStats.simdBackend = useAvx2 ? "avx2" : "scalar";
  localStats.totalBitmapMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bitmapStart).count();
  if (stats != nullptr) {
    *stats = std::move(localStats);
  }
  if (!placements.empty()) {
    out.placements.push_back(SheetPlacement{sheet.source, sheet.id, placements});
  }
  out.utilisation = out.totalarea > 0.0 ? (out.area / out.totalarea) * 100.0 : 0.0;
  out.fitness = static_cast<double>(out.unplaced.size());
  return out;
}

}  // namespace

bool bitmapAvx2Supported() {
#if defined(__AVX2__)
  return avx2RuntimeAvailable();
#else
  return false;
#endif
}

PlacementResult placePartsBitmap(const std::vector<Polygon>& sheets,
                                 const std::vector<Polygon>& parts,
                                 const Config& config,
                                 BitmapNestingStats* stats,
                                 const BitmapPartProgressCallback& onPartProgress) {
  if (config.bitmapResolutionMm <= 0.0) {
    throw std::invalid_argument("--bitmap-resolution must be a positive number");
  }
  if (config.bitmapSearchStepPx <= 0) {
    throw std::invalid_argument("--bitmap-step must be a positive integer");
  }
  if (sheets.empty()) {
    return {};
  }

  const bool useAvx2 =
#if defined(__AVX2__)
      config.bitmapPreferAvx2 && avx2RuntimeAvailable();
#else
      false;
#endif

  BitmapNestingStats localStats;
  PlacementResult result =
      placePartsBitmapOnSingleSheet(sheets.front(), parts, config, useAvx2, &localStats, onPartProgress);
  if (stats != nullptr) {
    *stats = localStats;
  }
  return result;
}

}  // namespace deepnest

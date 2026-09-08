#pragma once

#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace deepnest {

enum class NestingAlgorithm { Nfp, Bitmap };

inline int defaultWorkerCount() {
  const unsigned int detected = std::thread::hardware_concurrency();
  return detected == 0U ? 1 : static_cast<int>(detected);
}

inline int normalizeWorkerCount(int requested) {
  return requested >= 1 ? requested : 1;
}

struct Point {
  double x{0.0};
  double y{0.0};
  bool exact{true};
};

struct Segment {
  Point a;
  Point b;
};

struct Polygon {
  std::vector<Point> points;
  std::vector<Polygon> children;

  std::optional<int> id;
  std::string source;
  std::string filename;
  std::string geometryKey;
  double rotation{0.0};
};

struct Bounds {
  double x{0.0};
  double y{0.0};
  double width{0.0};
  double height{0.0};
};

struct Config {
  int rotations{4};
  int threads{defaultWorkerCount()};
  NestingAlgorithm algorithm{NestingAlgorithm::Nfp};
  double bitmapResolutionMm{1.0};
  int bitmapSearchStepPx{1};
  bool bitmapPreferAvx2{true};
  bool bitmapValidateGeometry{true};
  bool debugPlacement{false};
  double spacing{0.0};
  bool simplify{false};
  std::string placementType{"gravity"};
  bool mergeLines{false};
  double clipperScale{10000000.0};
  double scale{1.0};
  double curveTolerance{0.3};
  double timeRatio{1.0};
};

struct Placement {
  double x{0.0};
  double y{0.0};
  std::optional<int> id;
  double rotation{0.0};
  std::string source;
  std::string filename;
  double mergedLength{0.0};
  std::vector<Segment> mergedSegments;
};

struct SheetPlacement {
  std::string sheet;
  std::optional<int> sheetid;
  std::vector<Placement> sheetplacements;
};

struct PlacementResult {
  std::vector<SheetPlacement> placements;
  double fitness{0.0};
  double area{0.0};
  double totalarea{0.0};
  double mergedLength{0.0};
  double utilisation{0.0};
  std::vector<Polygon> unplaced;
};

struct MergedLengthResult {
  double totalLength{0.0};
  std::vector<Segment> segments;
};

}  // namespace deepnest

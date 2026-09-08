#include "deepnestcpp/nfp.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>

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

TEST_CASE("thread-safe cache warm-up computes each unique pair once") {
  Config cfg;
  cfg.threads = 4;
  NfpCache cache;

  std::vector<Polygon> parts;
  for (int i = 0; i < 4; ++i) {
    Polygon part = rect(0, 0, 2.0 + i, 2.0 + (i % 2), "p" + std::to_string(i));
    part.id = i + 1;
    parts.push_back(part);
  }

  const auto pairs = preprocessMissingPairs(parts, cache);
  REQUIRE_FALSE(pairs.empty());

  std::atomic<size_t> nextIndex{0};
  std::atomic<bool> allComputed{true};
  const size_t workerCount = std::min(pairs.size(), static_cast<size_t>(cfg.threads));
  std::vector<std::thread> workers;
  workers.reserve(workerCount);
  for (size_t worker = 0; worker < workerCount; ++worker) {
    workers.emplace_back([&]() {
      while (true) {
        const size_t pairIndex = nextIndex.fetch_add(1);
        if (pairIndex >= pairs.size()) {
          break;
        }
        if (!getOuterNfp(pairs[pairIndex].A, pairs[pairIndex].B, false, cfg, cache).has_value()) {
          allComputed = false;
        }
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }

  REQUIRE(allComputed);
  REQUIRE(cache.outerStoreCount() == pairs.size());
}

TEST_CASE("thread-safe cache deduplicates concurrent inserts for the same key") {
  NfpCache cache;
  const NfpKey outerKey{"A", "B", 0.0, 0.0, false};
  const NfpKey innerKey{"S", "P", 0.0, 0.0, true};
  const Polygon outerValue = rect(0, 0, 4, 4, "outer");
  const std::vector<Polygon> innerValue{rect(0, 0, 2, 2, "inner")};
  std::atomic<bool> readsStayedValid{true};

  std::vector<std::thread> workers;
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&]() {
      for (int iteration = 0; iteration < 50; ++iteration) {
        cache.insertOuter(outerKey, outerValue);
        cache.insertInner(innerKey, innerValue);
        if (!cache.has(outerKey) || !cache.has(innerKey) || !cache.findOuter(outerKey).has_value() ||
            !cache.findInner(innerKey).has_value()) {
          readsStayedValid = false;
        }
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }

  REQUIRE(readsStayedValid);
  REQUIRE(cache.outerStoreCount() == 1);
  REQUIRE(cache.innerStoreCount() == 1);
}

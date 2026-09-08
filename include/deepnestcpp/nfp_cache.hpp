#pragma once

#include "deepnestcpp/model.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace deepnest {

struct NfpKey {
  std::string A;
  std::string B;
  double Arotation{0.0};
  double Brotation{0.0};
  bool inner{false};

  bool operator==(const NfpKey& other) const;
};

struct NfpKeyHash {
  std::size_t operator()(const NfpKey& key) const;
};

class NfpCache {
 public:
  bool has(const NfpKey& key) const;
  std::optional<Polygon> findOuter(const NfpKey& key) const;
  std::optional<std::vector<Polygon>> findInner(const NfpKey& key) const;
  void insertOuter(const NfpKey& key, const Polygon& nfp);
  void insertInner(const NfpKey& key, const std::vector<Polygon>& nfp);

 private:
  std::unordered_map<NfpKey, Polygon, NfpKeyHash> outer_;
  std::unordered_map<NfpKey, std::vector<Polygon>, NfpKeyHash> inner_;
};

}  // namespace deepnest

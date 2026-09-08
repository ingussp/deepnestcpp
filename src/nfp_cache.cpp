#include "deepnestcpp/nfp_cache.hpp"

#include <cmath>
#include <functional>

namespace deepnest {

namespace {

inline long long rotKey(double v) {
  return static_cast<long long>(std::llround(v * 1000000.0));
}

}  // namespace

bool NfpKey::operator==(const NfpKey& other) const {
  return A == other.A && B == other.B && rotKey(Arotation) == rotKey(other.Arotation) &&
         rotKey(Brotation) == rotKey(other.Brotation) && inner == other.inner;
}

std::size_t NfpKeyHash::operator()(const NfpKey& key) const {
  std::size_t h1 = std::hash<std::string>{}(key.A);
  std::size_t h2 = std::hash<std::string>{}(key.B);
  std::size_t h3 = std::hash<long long>{}(rotKey(key.Arotation));
  std::size_t h4 = std::hash<long long>{}(rotKey(key.Brotation));
  std::size_t h5 = std::hash<bool>{}(key.inner);
  return (((h1 * 1315423911u) ^ h2) * 2654435761u) ^ h3 ^ (h4 << 1) ^ (h5 << 2);
}

bool NfpCache::has(const NfpKey& key) const {
  if (key.inner) {
    return inner_.find(key) != inner_.end();
  }
  return outer_.find(key) != outer_.end();
}

std::optional<Polygon> NfpCache::findOuter(const NfpKey& key) const {
  auto it = outer_.find(key);
  if (it == outer_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::vector<Polygon>> NfpCache::findInner(const NfpKey& key) const {
  auto it = inner_.find(key);
  if (it == inner_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void NfpCache::insertOuter(const NfpKey& key, const Polygon& nfp) {
  ++outerStoreCount_;
  outer_[key] = nfp;
}

void NfpCache::insertInner(const NfpKey& key, const std::vector<Polygon>& nfp) {
  ++innerStoreCount_;
  inner_[key] = nfp;
}

size_t NfpCache::outerStoreCount() const {
  return outerStoreCount_;
}

size_t NfpCache::innerStoreCount() const {
  return innerStoreCount_;
}

}  // namespace deepnest

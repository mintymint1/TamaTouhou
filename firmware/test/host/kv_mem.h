#pragma once
// In-memory KvStore: stands in for NVS. Survives "reboots" (a new Pet on the same store).
#include <map>
#include <string>
#include <vector>

#include "platform.h"

class MemKv : public tama::KvStore {
 public:
  size_t getBytes(const char *key, void *buf, size_t maxLen) override {
    auto it = m_.find(key);
    if (it == m_.end() || it->second.size() > maxLen) return 0;
    std::memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }
  bool putBytes(const char *key, const void *buf, size_t len) override {
    if (failWrites > 0) { failWrites--; return false; }   // a full or failing flash
    m_[key].assign((const uint8_t *)buf, (const uint8_t *)buf + len);
    writes++;
    return true;
  }
  void remove(const char *key) override { m_.erase(key); }
  size_t size(const char *key) const {
    auto it = m_.find(key);
    return it == m_.end() ? 0 : it->second.size();
  }
  int writes = 0;
  int failWrites = 0;   // the next N writes fail

 private:
  std::map<std::string, std::vector<uint8_t>> m_;
};

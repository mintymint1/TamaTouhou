#pragma once
// The few things the engine needs from the outside world, as interfaces, so the same code
// runs on the ESP32 (NVS, esp_random) and in host tests (in-memory map, seeded RNG).
#include <cstddef>
#include <cstdint>

namespace tama {

// Persistent key/value storage. On the device: NVS via Preferences. Writing freezes both
// cores for ~1 s on ESP32, so the engine batches everything into ONE blob per save.
class KvStore {
 public:
  virtual ~KvStore() = default;
  virtual size_t getBytes(const char *key, void *buf, size_t maxLen) = 0;  // 0 = missing
  virtual bool putBytes(const char *key, const void *buf, size_t len) = 0;
  virtual void remove(const char *key) = 0;
};

// Small fast PRNG (xorshift32). Seed it from esp_random() on the device, fixed in tests.
class Rng {
 public:
  explicit Rng(uint32_t seed = 0x9E3779B9u) : s_(seed ? seed : 1) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }         // [0, n)
  float percent() { return (next() >> 8) * (100.0f / 16777216.0f); }  // [0, 100)
  void seed(uint32_t s) { s_ = s ? s : 1; }

 private:
  uint32_t s_;
};

// Milliseconds left until `deadline` (0 if passed, or if deadline == 0 = inactive).
// The signed difference stays correct across the millis() wrap at ~49.7 days, as long as
// intervals are far below ~24.8 days (TamaPoke changelog 1.9).
inline uint32_t timeLeft(uint32_t deadline, uint32_t now) {
  if (!deadline) return 0;
  int32_t left = (int32_t)(deadline - now);
  return left > 0 ? (uint32_t)left : 0;
}

// A deadline `ms` from now that is never 0 (0 means "inactive").
inline uint32_t deadlineIn(uint32_t now, uint32_t ms) {
  uint32_t d = now + ms;
  return d ? d : 1;
}

}  // namespace tama

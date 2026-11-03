#pragma once
// tama::KvStore over ESP32 NVS (Preferences). One namespace per theme so switching themes
// never touches another theme's pet (docs/themes.md).
#include <Preferences.h>

#include "platform.h"

class NvsKv : public tama::KvStore {
 public:
  explicit NvsKv(const char *ns) { prefs_.begin(ns, false); }
  size_t getBytes(const char *key, void *buf, size_t maxLen) override {
    size_t len = prefs_.getBytesLength(key);
    if (!len || len > maxLen) return 0;
    return prefs_.getBytes(key, buf, len);
  }
  bool putBytes(const char *key, const void *buf, size_t len) override {
    return prefs_.putBytes(key, buf, len) == len;
  }
  void remove(const char *key) override { prefs_.remove(key); }

 private:
  Preferences prefs_;
};

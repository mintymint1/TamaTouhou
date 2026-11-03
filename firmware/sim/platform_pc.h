#pragma once
// The PC side of app_platform.h (plat::*) for the simulator: SDL3 audio, asset files from a
// folder, saves as one file per key, the wall clock. Portable C++17 + SDL3 only, so it builds
// on macOS, Linux and Windows.
#include <cstdint>
#include <string>

#include "platform.h"

namespace pc {

void setDataDir(const std::string &dir);   // where /sprites, /items, /bg live (firmware/data)
bool audioOpen();                          // after SDL_Init(SDL_INIT_AUDIO); false = silent
void audioClose();
void setMuted(bool m);                     // the simulator's M key
bool muted();

// One file per key under dir: the PC stand-in for NVS. Writes are atomic (temp + rename).
class FileKv : public tama::KvStore {
 public:
  explicit FileKv(std::string dir);
  size_t getBytes(const char *key, void *buf, size_t maxLen) override;
  bool putBytes(const char *key, const void *buf, size_t len) override;
  void remove(const char *key) override;
  const std::string &dir() const { return dir_; }

 private:
  std::string path(const char *key) const;
  std::string dir_;
};

// "t_<hash16(theme id)>": the same per-theme save namespace as the board's NVS.
std::string themeNamespace();

}  // namespace pc

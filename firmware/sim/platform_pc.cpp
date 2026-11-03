#include "platform_pc.h"

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <random>
#include <vector>

#include "app_platform.h"
#include "gfx.h"
#include "sfx_table.h"
#include "theme_data.h"

namespace {
std::string gDataDir = "data";
SDL_AudioStream *gAudio = nullptr;
bool gMuted = false;
bool gSound = true;   // the Settings screen's sound switch
constexpr int kSampleRate = 16000;
long gClockOffset = 0;   // Settings can set the clock: offset from the system time
uint8_t gRotation = 0;

// The board's RTC holds LOCAL wall-clock time (set from the Settings screen), so the game's
// day/night and clock read local time. Mirror that: local time expressed as if it were UTC.
// The offset is the difference between the local and UTC calendar fields (portable, unlike
// tm_gmtoff, which Windows lacks).
long localNow() {
  std::time_t t = std::time(nullptr);
  std::tm lt = *std::localtime(&t), ut = *std::gmtime(&t);
  long off = (lt.tm_hour - ut.tm_hour) * 3600L + (lt.tm_min - ut.tm_min) * 60L + (lt.tm_sec - ut.tm_sec);
  int days = lt.tm_yday - ut.tm_yday;
  if (days > 1) days = -1;   // across New Year
  if (days < -1) days = 1;
  return (long)t + off + days * 86400L;
}
}  // namespace

// --------------------------------------------------------------------------- plat::* for the app layer

namespace plat {

void sfx(uint8_t id) {
  if (!gAudio || gMuted || !gSound || id >= SFX_COUNT) return;
  if (SDL_GetAudioStreamQueued(gAudio) > kSampleRate) return;   // >0.5 s queued: drop, like the board's full queue
  std::vector<int16_t> buf;
  const sfxdata::Def &d = sfxdata::kSfx[id];
  for (uint8_t i = 0; i < d.len; i++) {
    std::vector<int16_t> note(kSampleRate);
    int n = sfxdata::renderNote(d.n[i], kSampleRate, note.data(), (int)note.size(), 3000);
    buf.insert(buf.end(), note.begin(), note.begin() + n);
  }
  SDL_PutAudioStreamData(gAudio, buf.data(), (int)(buf.size() * sizeof(int16_t)));
}

uint32_t random32() {
  static std::mt19937 rng(std::random_device{}());
  return rng();
}

// SDL's file calls take UTF-8 paths on every OS (fopen doesn't on Windows).
bool readFile(const char *path, uint8_t **data, size_t *len, size_t maxLen) {
  std::string full = gDataDir + path;
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(full.c_str(), &info) || info.type != SDL_PATHTYPE_FILE) return false;
  if (info.size == 0 || info.size > maxLen) return false;
  size_t n = 0;
  void *p = SDL_LoadFile(full.c_str(), &n);
  if (!p || n == 0 || n > maxLen) { SDL_free(p); return false; }
  *data = (uint8_t *)p;
  *len = n;
  return true;
}

void freeFile(uint8_t *data) { SDL_free(data); }

void log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::fflush(stdout);
}

// The computer's own battery (a laptop), or none (a desktop). Cached: SDL asks the OS.
Power power() {
  static Power cached;
  static uint64_t at = 0;
  uint64_t now = SDL_GetTicks();
  if (at && now - at < 2000) return cached;
  at = now ? now : 1;
  int pct = -1;
  SDL_PowerState st = SDL_GetPowerInfo(nullptr, &pct);
  Power p;
  p.percent = (st == SDL_POWERSTATE_ON_BATTERY || st == SDL_POWERSTATE_CHARGING || st == SDL_POWERSTATE_CHARGED)
                  ? (int8_t)(pct < 0 ? -1 : pct > 100 ? 100 : pct) : -1;
  p.charging = st == SDL_POWERSTATE_CHARGING;
  p.external = st == SDL_POWERSTATE_CHARGING || st == SDL_POWERSTATE_CHARGED || st == SDL_POWERSTATE_NO_BATTERY;
  cached = p;
  return p;
}

uint32_t epoch() { return (uint32_t)(localNow() + gClockOffset); }
void setEpoch(uint32_t e) { gClockOffset = (long)e - localNow(); }
void setSoundEnabled(bool on) { gSound = on; }
void setBrightnessLevel(uint8_t b) { (void)b; }   // no backlight on a PC
uint8_t rotation() { return gRotation; }
void setRotation(uint8_t q) {
  gRotation = q & 3;
  if (app::gfx) app::gfx->setRotation(gRotation);
}

}  // namespace plat

// --------------------------------------------------------------------------- pc::*

namespace pc {

void setDataDir(const std::string &dir) { gDataDir = dir; }

bool audioOpen() {
  SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, kSampleRate};
  gAudio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!gAudio) return false;
  SDL_ResumeAudioStreamDevice(gAudio);
  return true;
}

void audioClose() {
  if (gAudio) SDL_DestroyAudioStream(gAudio);
  gAudio = nullptr;
}

void setMuted(bool m) { gMuted = m; }
bool muted() { return gMuted; }

FileKv::FileKv(std::string dir) : dir_(std::move(dir)) { SDL_CreateDirectory(dir_.c_str()); }

std::string FileKv::path(const char *key) const { return dir_ + "/" + key + ".bin"; }

size_t FileKv::getBytes(const char *key, void *buf, size_t maxLen) {
  SDL_IOStream *f = SDL_IOFromFile(path(key).c_str(), "rb");
  if (!f) return 0;
  Sint64 size = SDL_GetIOSize(f);
  size_t n = (size > 0 && (uint64_t)size <= maxLen) ? SDL_ReadIO(f, buf, (size_t)size) : 0;
  SDL_CloseIO(f);
  return (Sint64)n == size ? n : 0;   // missing, too big or short read = missing
}

bool FileKv::putBytes(const char *key, const void *buf, size_t len) {
  std::string tmp = path(key) + ".tmp";
  if (!SDL_SaveFile(tmp.c_str(), buf, len)) return false;
  return SDL_RenamePath(tmp.c_str(), path(key).c_str());   // atomic replace
}

void FileKv::remove(const char *key) { SDL_RemovePath(path(key).c_str()); }

std::string themeNamespace() {
  char ns[16];
  std::snprintf(ns, sizeof(ns), "t_%04x", tama::hash16(tama::kTheme.id));
  return ns;
}

}  // namespace pc

#include "save_file.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <ctime>
#include <vector>

#include "app_platform.h"
#include "platform_pc.h"
#include "save_text.h"
#include "theme_data.h"

namespace pc {

std::string defaultExportName() {
  std::time_t t = std::time(nullptr);
  char day[16];
  std::strftime(day, sizeof(day), "%Y-%m-%d", std::localtime(&t));
  return std::string("tamatouhou-") + tama::kTheme.id + "-" + day + "." + kSaveFileExt;
}

bool exportSaveFile(tama::Pet &pet, const std::string &path, std::string &err) {
  std::vector<uint8_t> pkg(tama::Pet::kExportMax);
  size_t n = pet.exportSave(pkg.data(), pkg.size());
  if (!n) { err = "the save didn't fit the export buffer"; return false; }
  std::vector<char> text(tama::saveTextCapacity(n, tama::kTheme.id));
  size_t len = tama::encodeSaveText(pkg.data(), n, tama::kTheme.id, text.data(), text.size(), false);
  if (!len) { err = "couldn't encode the save"; return false; }
  if (!SDL_SaveFile(path.c_str(), text.data(), len)) {   // UTF-8 path on every OS
    err = std::string("couldn't write ") + path + ": " + SDL_GetError();
    return false;
  }
  return true;
}

bool importSaveFile(tama::Pet &pet, const std::string &path, uint32_t nowMs, uint32_t nowEpoch, std::string &err) {
  size_t size = 0;
  void *raw = SDL_LoadFile(path.c_str(), &size);
  if (!raw) { err = std::string("couldn't read ") + path + ": " + SDL_GetError(); return false; }
  std::vector<uint8_t> pkg(tama::Pet::kExportMax);
  char theme[40];
  size_t n = size < 64 * 1024 ? tama::decodeSaveText((const char *)raw, size, pkg.data(), pkg.size(), theme, sizeof(theme)) : 0;
  SDL_free(raw);
  if (!n) { err = "that file isn't a TamaTouhou save"; return false; }
  if (std::strcmp(theme, tama::kTheme.id) != 0) {
    err = std::string("that save is for the \"") + theme + "\" theme, this game runs \"" + tama::kTheme.id + "\"";
    return false;
  }
  switch (pet.importSave(pkg.data(), n, nowMs)) {
    case tama::ImportResult::Ok: break;
    case tama::ImportResult::WrongTheme: err = "that save is for another theme"; return false;
    case tama::ImportResult::Corrupt: err = "that save is damaged (checksum or contents don't match)"; return false;
    default: err = "that file isn't a TamaTouhou save"; return false;
  }
  pet.syncClock(nowEpoch);   // the time since the export passes like time powered off
  return true;
}

}  // namespace pc

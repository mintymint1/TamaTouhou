#pragma once
// Save export/import as a TAMA-SAVE text file (lib/engine/save_text.h) for the simulator.
// The same file imports on the board over the serial
// console (tools/board_save.py), as long as the theme matches.
#include <cstdint>
#include <string>

#include "pet.h"

namespace pc {

constexpr char kSaveFileExt[] = "tamasave";

// "tamatouhou-<theme>-<yyyy-mm-dd>.tamasave"
std::string defaultExportName();

// Writes the whole game (pet + storage). On failure `err` says why.
bool exportSaveFile(tama::Pet &pet, const std::string &path, std::string &err);

// Replaces the whole game with the file's, then applies the time since it was exported (like
// time powered off, capped at 14 days). Nothing changes if it fails; `err` says why.
bool importSaveFile(tama::Pet &pet, const std::string &path, uint32_t nowMs, uint32_t nowEpoch, std::string &err);

}  // namespace pc

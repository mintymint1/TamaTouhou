#pragma once
// Text form of a save package (Pet::exportSave), so a save fits in a file, a clipboard or one
// serial-console line:
//
//   TAMA-SAVE 1 <theme id>
//   <base64 of the package, wrapped at 76 columns or all on one line>
//
// Decoding ignores whitespace and line breaks, so either layout reads back. Portable C++17,
// no allocation: shared by the board's serial console and the PC front ends.
#include <cstddef>
#include <cstdint>

namespace tama {

constexpr char kSaveTextTag[] = "TAMA-SAVE";
constexpr int kSaveTextVersion = 1;

// Buffer size encodeSaveText() needs for `binLen` bytes, terminator included.
size_t saveTextCapacity(size_t binLen, const char *themeId);

// Writes the text form (NUL-terminated). Returns its length, or 0 if `cap` is too small.
size_t encodeSaveText(const uint8_t *bin, size_t binLen, const char *themeId, char *out, size_t cap, bool oneLine);

// Reads the text form back into `out`. Returns the package length, or 0 if the text isn't a
// save (wrong tag/version, bad base64, or too big for `cap`). The header's theme id is copied
// to `themeOut` (truncated to themeCap - 1) so the caller can name the theme in an error.
size_t decodeSaveText(const char *text, size_t textLen, uint8_t *out, size_t cap, char *themeOut, size_t themeCap);

}  // namespace tama

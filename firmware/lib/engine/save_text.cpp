#include "save_text.h"

#include <cstdio>
#include <cstring>

namespace tama {

namespace {

constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr size_t kWrap = 76;

int b64Value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

size_t saveTextCapacity(size_t binLen, const char *themeId) {
  size_t b64 = (binLen + 2) / 3 * 4;
  return sizeof(kSaveTextTag) + 4 + std::strlen(themeId ? themeId : "") + 1 + b64 + b64 / kWrap + 2;
}

size_t encodeSaveText(const uint8_t *bin, size_t binLen, const char *themeId, char *out, size_t cap, bool oneLine) {
  if (!out || cap < saveTextCapacity(binLen, themeId)) return 0;
  int n = std::snprintf(out, cap, "%s %d %s%c", kSaveTextTag, kSaveTextVersion, themeId ? themeId : "",
                        oneLine ? ' ' : '\n');
  if (n < 0 || (size_t)n >= cap) return 0;
  size_t o = (size_t)n, col = 0;
  for (size_t i = 0; i < binLen; i += 3) {
    uint32_t v = (uint32_t)bin[i] << 16;
    if (i + 1 < binLen) v |= (uint32_t)bin[i + 1] << 8;
    if (i + 2 < binLen) v |= bin[i + 2];
    out[o++] = kB64[(v >> 18) & 63];
    out[o++] = kB64[(v >> 12) & 63];
    out[o++] = i + 1 < binLen ? kB64[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < binLen ? kB64[v & 63] : '=';
    col += 4;
    if (!oneLine && col >= kWrap && i + 3 < binLen) {
      out[o++] = '\n';
      col = 0;
    }
  }
  if (!oneLine) out[o++] = '\n';
  out[o] = 0;
  return o;
}

size_t decodeSaveText(const char *text, size_t textLen, uint8_t *out, size_t cap, char *themeOut, size_t themeCap) {
  if (themeOut && themeCap) themeOut[0] = 0;
  if (!text) return 0;
  size_t i = 0;
  while (i < textLen && isSpace(text[i])) i++;
  size_t tagLen = sizeof(kSaveTextTag) - 1;
  if (textLen - i < tagLen || std::strncmp(text + i, kSaveTextTag, tagLen) != 0) return 0;
  i += tagLen;
  // version
  while (i < textLen && isSpace(text[i])) i++;
  int version = 0;
  while (i < textLen && text[i] >= '0' && text[i] <= '9') version = version * 10 + (text[i++] - '0');
  if (version != kSaveTextVersion) return 0;
  // theme id
  while (i < textLen && isSpace(text[i])) i++;
  size_t t = 0;
  while (i < textLen && !isSpace(text[i])) {
    if (themeOut && t + 1 < themeCap) themeOut[t++] = text[i];
    i++;
  }
  if (themeOut && themeCap) themeOut[t < themeCap ? t : themeCap - 1] = 0;
  // base64 body: whitespace anywhere, '=' padding only at the end
  uint32_t acc = 0;
  int bits = 0;
  size_t o = 0;
  bool padded = false;
  for (; i < textLen && text[i]; i++) {
    char c = text[i];
    if (isSpace(c)) continue;
    if (c == '=') { padded = true; continue; }
    int v = b64Value(c);
    if (v < 0 || padded) return 0;
    acc = (acc << 6) | (uint32_t)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (o >= cap) return 0;
      out[o++] = (uint8_t)(acc >> bits);
    }
  }
  return o;
}

}  // namespace tama

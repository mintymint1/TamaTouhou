#pragma once
// CST9217 capacitive touch on the shared I2C bus.
#include <cstdint>

namespace hw {

bool touchBegin();
// Polls at most every 20 ms. Returns true when a sample was read (pressed may be false =
// finger lifted). Only touches the bus after an INT or while a finger is down: reading the
// sleeping CST9217 hangs ~1 s (TamaPoke 1.11).
bool touchRead(bool &pressed, int16_t &x, int16_t &y);

}  // namespace hw

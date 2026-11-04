#include "Controller.hpp"

void Controller::set(Button button, bool pressed) {
    if (pressed) {
        state = static_cast<uint8_t>(state | (1u << button));
    } else {
        state = static_cast<uint8_t>(state & ~(1u << button));
    }
    // While the strobe line is held high the shift register follows the
    // buttons live (matches hardware: reload happens every PPU cycle).
    if (strobe) {
        latch();
    }
}

void Controller::setMask(uint8_t mask) {
    state = mask;
    if (strobe) {
        latch();
    }
}

void Controller::write(uint8_t data) {
    const bool newStrobe = (data & 1) != 0;
    if (newStrobe) {
        latch(); // continuously reload while strobe is high
    } else if (strobe) {
        latch(); // falling edge: freeze the state present at release
    }
    strobe = newStrobe;
}

uint8_t Controller::read() {
    if (strobe) {
        // Strobe still high: always reports A (register keeps reloading)
        return static_cast<uint8_t>(state & 1u);
    }
    const uint8_t bit = static_cast<uint8_t>(shift & 1u);
    // Shift right; fill with 1s so reads 9+ return 1 like the real pad
    shift = static_cast<uint8_t>((shift >> 1) | 0x80);
    return bit;
}

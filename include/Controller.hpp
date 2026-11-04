#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <cstdint>

// Standard NES controller: an 8-bit parallel-load shift register.
// The CPU strobes $4016 bit 0 high then low; each read of $4016/$4017
// clocks out one button bit in fixed order:
//   A, B, Select, Start, Up, Down, Left, Right
// Reads after the first eight return 1 (official controller behavior).
class Controller {
public:
    enum Button : uint8_t {
        BTN_A = 0,
        BTN_B,
        BTN_SELECT,
        BTN_START,
        BTN_UP,
        BTN_DOWN,
        BTN_LEFT,
        BTN_RIGHT,
    };

    void set(Button button, bool pressed);
    void setMask(uint8_t mask); // bit i = button i

    // Strobe line ($4016 bit 0): high reloads the shift register continuously,
    // the falling edge latches the current button state.
    void write(uint8_t data);

    // One serial clock; returns the next button bit in bit 0
    uint8_t read();

    bool isPressed(Button button) const { return state & (1u << button); }

private:
    void latch() { shift = state; }

    uint8_t state = 0; // live button bitmap
    uint8_t shift = 0; // latched serial output
    bool strobe = false;
};

#endif // CONTROLLER_HPP

#ifndef CPU_HPP
#define CPU_HPP

#include <cstdint>

class Bus;

class CPU {
public:
    // Status flag bit masks (P register)
    static constexpr uint8_t FLAG_C = 1 << 0; // Carry
    static constexpr uint8_t FLAG_Z = 1 << 1; // Zero
    static constexpr uint8_t FLAG_I = 1 << 2; // Interrupt disable
    static constexpr uint8_t FLAG_D = 1 << 3; // Decimal (unused on NES)
    static constexpr uint8_t FLAG_B = 1 << 4; // Break
    static constexpr uint8_t FLAG_U = 1 << 5; // Unused (hardware forces this to 1)
    static constexpr uint8_t FLAG_V = 1 << 6; // Overflow
    static constexpr uint8_t FLAG_N = 1 << 7; // Negative

    explicit CPU(Bus& bus);

    // Power-on / RESET line: loads PC from the reset vector at 0xFFFC
    void reset();

    // Flag helpers
    bool getFlag(uint8_t mask) const;
    void setFlag(uint8_t mask, bool value);

    // Stack operations (stack page = 0x0100..0x01FF, grows downward)
    void push8(uint8_t value);
    uint8_t pop8();
    void push16(uint16_t value);
    uint16_t pop16();

    // Bus access used by the CPU core
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t data);
    uint16_t read16(uint16_t addr) const;

    // Registers (public so tests and the debugger can inspect them)
    uint8_t A = 0;
    uint8_t X = 0;
    uint8_t Y = 0;
    uint16_t PC = 0;
    uint8_t SP = 0;
    uint8_t P = 0;

private:
    Bus* bus;
};

#endif // CPU_HPP

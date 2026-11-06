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
    static constexpr uint8_t FLAG_B = 1 << 4; // Break (stack-only, never stored in P)
    static constexpr uint8_t FLAG_U = 1 << 5; // Unused (hardware forces this to 1)
    static constexpr uint8_t FLAG_V = 1 << 6; // Overflow
    static constexpr uint8_t FLAG_N = 1 << 7; // Negative

    explicit CPU(Bus& bus);

    // Power-on / RESET line: loads PC from the reset vector at 0xFFFC
    void reset();

    // Non-maskable interrupt: pushes PC+P, loads PC from 0xFFFA
    void nmi();

    // Maskable interrupt: pushes PC+P, loads PC from 0xFFFE (if I clear)
    void irq();

    // Fetch, decode and execute one instruction; advances total_cycles
    void step();

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

    // Set by CLI/SEI/PLP for one instruction: the interrupt poll still sees
    // the pre-instruction I flag (6502's one-instruction flag delay).
    // Cleared at the start of every step(); consumed by System::step().
    bool irqPollDelay = false;

    // Total CPU cycles since power-on (reset leaves this at 7, matching nestest)
    uint64_t total_cycles = 0;

private:
    Bus* bus;

    // Set by indexed addressing modes when the base and effective addresses
    // differ in the high byte (adds 1 cycle to read instructions)
    bool page_crossed = false;

    // --- Flag / ALU helpers ---
    void setZN(uint8_t value);
    void opADC(uint8_t value);
    void opSBC(uint8_t value);
    void opCMP(uint8_t reg, uint8_t value);
    void opBIT(uint8_t value);
    uint8_t opASL(uint8_t value);
    uint8_t opLSR(uint8_t value);
    uint8_t opROL(uint8_t value);
    uint8_t opROR(uint8_t value);

    // --- Addressing modes (advance PC past operands; set page_crossed) ---
    uint16_t addrImm();
    uint16_t addrZp();
    uint16_t addrZpX();
    uint16_t addrZpY();
    uint16_t addrAbs();
    uint16_t addrAbsX();
    uint16_t addrAbsY();
    uint16_t addrIndX();  // (zp,X)
    uint16_t addrIndY();  // (zp),Y
    uint16_t addrAbsX_Write(); // abs,X for stores and RMW (always dummy reads uncorrected)
    uint16_t addrAbsY_Write(); // abs,Y for stores and RMW (always dummy reads uncorrected)
    uint16_t addrIndY_Write(); // (zp),Y for stores and RMW (always dummy reads uncorrected)
    uint16_t addrInd();   // JMP ($xxxx) with the 6502 page-wrap bug
};

#endif // CPU_HPP

#include "CPU.hpp"
#include "Bus.hpp"

CPU::CPU(Bus& bus) : bus(&bus) {}

void CPU::reset() {
    A = X = Y = 0;
    SP = 0xFD;               // NES convention: stack starts at top of page 1 minus 3
    P = FLAG_U | FLAG_I;     // 0x24: unused bit always 1, IRQs masked during reset
    // The cartridge supplies a 16-bit little-endian start address at 0xFFFC/0xFFFD
    PC = read16(0xFFFC);
}

bool CPU::getFlag(uint8_t mask) const {
    return (P & mask) != 0;
}

void CPU::setFlag(uint8_t mask, bool value) {
    if (value) {
        P |= mask;
    } else {
        P &= ~mask;
    }
}

void CPU::push8(uint8_t value) {
    // 6502 push: store at 0x0100 + SP first, then decrement SP
    write(0x0100 + SP, value);
    SP--;
}

uint8_t CPU::pop8() {
    // 6502 pull: increment SP first, then read at 0x0100 + SP
    SP++;
    return read(0x0100 + SP);
}

void CPU::push16(uint16_t value) {
    // High byte pushed first so the low byte ends up at the lower address
    push8(static_cast<uint8_t>(value >> 8));
    push8(static_cast<uint8_t>(value & 0xFF));
}

uint16_t CPU::pop16() {
    uint8_t lo = pop8();
    uint8_t hi = pop8();
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint8_t CPU::read(uint16_t addr) const {
    return bus->cpuRead(addr);
}

void CPU::write(uint16_t addr, uint8_t data) {
    bus->cpuWrite(addr, data);
}

uint16_t CPU::read16(uint16_t addr) const {
    // Little-endian: low byte at addr, high byte at addr + 1
    uint16_t lo = read(addr);
    uint16_t hi = read(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

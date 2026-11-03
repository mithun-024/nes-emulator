#include <cassert>
#include <iostream>
#include "Bus.hpp"
#include "CPU.hpp"

// Reset must load PC from the vector at 0xFFFC/0xFFFD (little-endian)
void testResetVector() {
    Bus bus;
    CPU cpu(bus);

    bus.cpuWrite(0xFFFC, 0x34); // low byte
    bus.cpuWrite(0xFFFD, 0x12); // high byte

    cpu.reset();

    assert(cpu.PC == 0x1234);
    assert(cpu.SP == 0xFD);
    assert(cpu.getFlag(CPU::FLAG_I));      // IRQs masked after reset
    assert(cpu.getFlag(CPU::FLAG_U));      // unused flag always reads 1
    assert(cpu.A == 0 && cpu.X == 0 && cpu.Y == 0);
    std::cout << "[PASS] reset vector" << std::endl;
}

// Flag set/clear must only touch its own bit
void testFlags() {
    Bus bus;
    CPU cpu(bus);
    cpu.P = 0;

    cpu.setFlag(CPU::FLAG_C, true);
    assert(cpu.getFlag(CPU::FLAG_C));
    assert(cpu.P == 0x01);

    cpu.setFlag(CPU::FLAG_N, true);
    assert(cpu.P == 0x81);

    cpu.setFlag(CPU::FLAG_C, false);
    assert(cpu.P == 0x80);
    std::cout << "[PASS] status flags" << std::endl;
}

// Push stores at 0x0100 + SP then decrements; pull increments then reads
void testStack8() {
    Bus bus;
    CPU cpu(bus);
    cpu.reset();
    uint8_t spStart = cpu.SP; // 0xFD

    cpu.push8(0xAB);
    assert(cpu.SP == spStart - 1);              // 0xFC
    assert(bus.cpuRead(0x0100 + spStart) == 0xAB); // byte went to 0x01FD

    uint8_t v = cpu.pop8();
    assert(v == 0xAB);
    assert(cpu.SP == spStart);
    std::cout << "[PASS] 8-bit stack push/pop" << std::endl;
}

// 16-bit push stores high byte first (matches JSR/RTS behavior)
void testStack16() {
    Bus bus;
    CPU cpu(bus);
    cpu.reset();

    cpu.push16(0x1234);
    // Two pushes from SP=0xFD: high at 0x01FD, low at 0x01FC, SP ends at 0xFB
    assert(cpu.SP == 0xFB);
    assert(bus.cpuRead(0x01FD) == 0x12);
    assert(bus.cpuRead(0x01FC) == 0x34);

    uint16_t v = cpu.pop16();
    assert(v == 0x1234);
    assert(cpu.SP == 0xFD);
    std::cout << "[PASS] 16-bit stack push/pop" << std::endl;
}

// 6502 is little-endian: read16 combines lo @ addr, hi @ addr+1
void testRead16() {
    Bus bus;
    CPU cpu(bus);
    bus.cpuWrite(0x8000, 0xCD);
    bus.cpuWrite(0x8001, 0xAB);
    assert(cpu.read16(0x8000) == 0xABCD);
    std::cout << "[PASS] little-endian read16" << std::endl;
}

void run_cpu_tests() {
    std::cout << "Running CPU tests..." << std::endl;
    testResetVector();
    testFlags();
    testStack8();
    testStack16();
    testRead16();
    std::cout << "CPU tests passed!" << std::endl;
}

#include <iostream>
#include "Bus.hpp"
#include "CPU.hpp"

int main() {
    std::cout << "NES Emulator - Milestone 2: 6502 CPU" << std::endl;

    Bus bus;
    CPU cpu(bus);

    // Plant a fake reset vector: game code starts at 0x8000
    bus.cpuWrite(0xFFFC, 0x00);
    bus.cpuWrite(0xFFFD, 0x80);

    cpu.reset();
    std::cout << "Reset vector at 0xFFFC -> PC = 0x" << std::hex << cpu.PC << std::endl;
    std::cout << "SP = 0x" << (int)cpu.SP << "  P = 0x" << (int)cpu.P << std::endl;

    return 0;
}

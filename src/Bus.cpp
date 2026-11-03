#include "Bus.hpp"
#include <iostream>

Bus::Bus() {
}

uint8_t Bus::cpuRead(uint16_t addr) const {
    // 0x0000 - 0x1FFF: 2KB Internal RAM (mirrored 4 times)
    if (addr <= 0x1FFF) {
        return ram.read(addr);
    }
    // 0x2000 - 0x3FFF: PPU Registers (8 registers mirrored every 8 bytes)
    else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // TODO: PPU read implementation in Milestone 5
        return 0x00;
    }
    // 0x4000 - 0x4017: APU & I/O Registers
    else if (addr >= 0x4000 && addr <= 0x4017) {
        // TODO: APU / Controllers implementation
        return 0x00;
    }
    // 0x4020 - 0xFFFF: Cartridge Space (PRG ROM, PRG RAM, Mapper)
    else if (addr >= 0x4020) {
        // TODO: Cartridge / Mapper read implementation in Milestone 4
        return 0x00;
    }

    return 0x00;
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    // 0x0000 - 0x1FFF: 2KB Internal RAM
    if (addr <= 0x1FFF) {
        ram.write(addr, data);
    }
    // 0x2000 - 0x3FFF: PPU Registers
    else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // TODO: PPU write implementation
    }
    // 0x4000 - 0x4017: APU & I/O Registers
    else if (addr >= 0x4000 && addr <= 0x4017) {
        // TODO: APU / Controller write implementation
    }
    // 0x4020 - 0xFFFF: Cartridge Space
    else if (addr >= 0x4020) {
        // TODO: Cartridge / Mapper write implementation
    }
}

#include "Mapper0.hpp"
#include "Cartridge.hpp"

Mapper0::Mapper0(Cartridge& cart) : Mapper(cart) {
    prgRam.resize(cart.prgRamSize(), 0);
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0); // CHR RAM (8KB)
    }
}

uint8_t Mapper0::cpuRead(uint16_t addr) const {
    // 0x4020–0x5FFF: expansion area — open bus (return 0 for now)
    if (addr < 0x6000) {
        return 0x00;
    }
    // 0x6000–0x7FFF: PRG RAM
    if (addr < 0x8000) {
        return prgRam[addr - 0x6000];
    }
    // 0x8000–0xFFFF: PRG ROM
    const auto& prg = cart.prgRom();
    const size_t offset = (prg.size() == 16384)
                              ? (addr & 0x3FFF)          // 16KB mirrored
                              : (addr - 0x8000);         // 32KB linear
    return prg[offset];
}

void Mapper0::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRam[addr - 0x6000] = data;
    }
    // Writes to PRG ROM are ignored (ROM is read-only)
}

uint8_t Mapper0::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF; // CHR is 8KB, mirrored across PPU pattern tables if smaller
    if (!cart.chrRom().empty()) {
        return cart.chrRom()[addr % cart.chrRom().size()];
    }
    return chrRam[addr];
}

void Mapper0::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        chrRam[addr] = data; // CHR RAM only; ROM writes are ignored
    }
}

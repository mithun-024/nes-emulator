#include "MapperCnrom.hpp"
#include "Cartridge.hpp"

MapperCnrom::MapperCnrom(Cartridge& cart) : Mapper(cart) {}

uint8_t MapperCnrom::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00; // expansion area
    if (addr < 0x8000) return 0x00; // no PRG RAM on CNROM

    const auto& prg = cart.prgRom();
    // Same mapping as NROM: 16KB mirrored at both halves, 32KB linear
    const size_t offset = (prg.size() == 16384)
                              ? (addr & 0x3FFF)
                              : (addr - 0x8000);
    return prg[offset % prg.size()];
}

void MapperCnrom::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0x8000) return;
    const auto& chr = cart.chrRom();
    const size_t banks = chr.empty() ? 0 : chr.size() / 8192;
    if (banks > 0) {
        chrBank = static_cast<uint8_t>(data % banks);
    }
}

uint8_t MapperCnrom::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    const auto& chr = cart.chrRom();
    if (chr.empty()) return 0x00; // CNROM is ROM-only CHR
    return chr[(static_cast<size_t>(chrBank) * 8192 + addr) % chr.size()];
}

void MapperCnrom::ppuWrite(uint16_t addr, uint8_t data) {
    (void)addr;
    (void)data; // CHR is ROM
}

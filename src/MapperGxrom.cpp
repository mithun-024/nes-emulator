#include "MapperGxrom.hpp"
#include "Cartridge.hpp"

MapperGxrom::MapperGxrom(Cartridge& cart) : Mapper(cart) {
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0);
    }
}

uint8_t MapperGxrom::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00; // expansion area
    if (addr < 0x8000) return 0x00; // no PRG RAM on GxROM

    const auto& prg = cart.prgRom();
    const size_t banks = prg.size() / 32768;
    const size_t bank = banks == 0 ? 0 : (prgBank % banks);
    return prg[(bank * 32768 + (addr - 0x8000)) % prg.size()];
}

void MapperGxrom::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0x8000) return;
    prgBank = static_cast<uint8_t>((data >> 4) & 0x03);
    chrBank = static_cast<uint8_t>(data & 0x03);
}

uint8_t MapperGxrom::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    const auto& chr = cart.chrRom();
    if (!chr.empty()) {
        return chr[(static_cast<size_t>(chrBank) * 8192 + addr) % chr.size()];
    }
    return chrRam[(static_cast<size_t>(chrBank) * 8192 + addr) % chrRam.size()];
}

void MapperGxrom::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        chrRam[(static_cast<size_t>(chrBank) * 8192 + addr) % chrRam.size()] = data;
    }
}

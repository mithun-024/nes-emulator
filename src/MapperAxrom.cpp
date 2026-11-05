#include "MapperAxrom.hpp"
#include "Bus.hpp"
#include "Cartridge.hpp"

MapperAxrom::MapperAxrom(Cartridge& cart) : Mapper(cart) {
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0);
    }
}

size_t MapperAxrom::bankCount() const {
    const size_t banks = cart.prgRom().size() / 32768;
    return banks == 0 ? 1 : banks;
}

uint8_t MapperAxrom::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00; // expansion area
    if (addr < 0x8000) return 0x00; // no PRG RAM on AxROM

    const auto& prg = cart.prgRom();
    const size_t offset = static_cast<size_t>(prgBank % bankCount()) * 32768 +
                          (addr - 0x8000);
    return prg[offset % prg.size()];
}

void MapperAxrom::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0x8000) return;
    prgBank = static_cast<uint8_t>(data & 0x0F);
    if (bus) {
        bus->setMirror((data & 0x10) ? PPU::Mirror::SingleHigh
                                     : PPU::Mirror::SingleLow);
    }
}

uint8_t MapperAxrom::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    if (!cart.chrRom().empty()) {
        return cart.chrRom()[addr % cart.chrRom().size()];
    }
    return chrRam[addr];
}

void MapperAxrom::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        chrRam[addr] = data;
    }
}

#include "Mapper2.hpp"
#include "Cartridge.hpp"

Mapper2::Mapper2(Cartridge& cart) : Mapper(cart) {
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0); // UxROM games almost always ship CHR RAM
    }
}

size_t Mapper2::bankCount() const {
    const size_t banks = cart.prgRom().size() / 16384;
    return banks == 0 ? 1 : banks;
}

uint8_t Mapper2::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00; // expansion area
    if (addr < 0x8000) return 0x00; // no PRG RAM on UxROM

    const auto& prg = cart.prgRom();
    // $8000–$BFFF: selected 16KB bank; $C000–$FFFF: fixed last bank
    const size_t bank = (addr < 0xC000) ? (prgBank % bankCount())
                                        : (bankCount() - 1);
    return prg[(bank * 16384 + (addr & 0x3FFF)) % prg.size()];
}

void Mapper2::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0x8000) return; // PRG RAM writes dropped too
    prgBank = static_cast<uint8_t>(data % bankCount());
}

uint8_t Mapper2::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    if (!cart.chrRom().empty()) {
        return cart.chrRom()[addr % cart.chrRom().size()];
    }
    return chrRam[addr];
}

void Mapper2::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        chrRam[addr] = data; // CHR RAM only; ROM writes are ignored
    }
}

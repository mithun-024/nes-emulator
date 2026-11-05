#include "MapperMmc2.hpp"
#include "Bus.hpp"
#include "Cartridge.hpp"

MapperMmc2::MapperMmc2(Cartridge& cart) : Mapper(cart) {
    if (cart.chrRom().empty()) {
        chrRam.resize(131072, 0); // 32 × 4KB, matches PxROM CHR size
    }
}

size_t MapperMmc2::prgBanks8() const {
    const size_t banks = cart.prgRom().size() / 8192;
    return banks == 0 ? 1 : banks;
}

size_t MapperMmc2::chrBanks4() const {
    const auto& chr = cart.chrRom();
    const size_t banks = chr.empty() ? (chrRam.size() / 4096)
                                     : (chr.size() / 4096);
    return banks == 0 ? 1 : banks;
}

uint8_t MapperMmc2::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00;
    if (addr < 0x8000) return 0x00; // no PRG RAM on MMC2

    const auto& prg = cart.prgRom();
    const size_t bank = (addr < 0xC000) ? (prgBank % prgBanks8())
                                        : (prgBanks8() - 1);
    return prg[(bank * 8192 + (addr & 0x1FFF)) % prg.size()];
}

void MapperMmc2::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0xA000) return;
    switch (addr & 0xF000) {
    case 0xA000: chrRegs[0] = data; break;
    case 0xB000: chrRegs[1] = data; break;
    case 0xC000: chrRegs[2] = data; break;
    case 0xD000: chrRegs[3] = data; break;
    case 0xE000: prgBank = data; break;
    default: // $F000–$FFFF: mirroring
        if (bus) {
            bus->setMirror((data & 1) ? PPU::Mirror::Horizontal
                                      : PPU::Mirror::Vertical);
        }
        break;
    }
}

uint8_t MapperMmc2::chrBankFor(uint16_t addr) const {
    // half 0 = $0000–$0FFF (latch 0), half 1 = $1000–$1FFF (latch 1)
    const bool second = addr >= 0x1000;
    const uint8_t latch = second ? latch1 : latch0;
    const uint8_t reg = chrRegs[(second ? 2 : 0) + (latch == 0xFE ? 1 : 0)];
    return static_cast<uint8_t>(reg % chrBanks4());
}

void MapperMmc2::updateLatches(uint16_t addr) const {
    // Pattern fetches of tile $FD/$FE (plane 0: $x0FD8–$x0FDF, $x0FE8–$x0FEF)
    if (addr >= 0x0FD8 && addr <= 0x0FDF) latch0 = 0xFD;
    else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch0 = 0xFE;
    else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch1 = 0xFD;
    else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch1 = 0xFE;
}

uint8_t MapperMmc2::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    // Fetching tile $FD/$FE flips the latch first, so both plane bytes of
    // that tile come from the newly selected bank.
    updateLatches(addr);
    const uint8_t bank = chrBankFor(addr);
    const auto& chr = cart.chrRom();
    const size_t offset = static_cast<size_t>(bank) * 4096 + (addr & 0xFFF);
    if (!chr.empty()) return chr[offset % chr.size()];
    return chrRam[offset % chrRam.size()];
}

void MapperMmc2::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        const size_t offset =
            static_cast<size_t>(chrBankFor(addr)) * 4096 + (addr & 0xFFF);
        chrRam[offset % chrRam.size()] = data;
    }
}

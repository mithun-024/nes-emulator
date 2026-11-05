#include "Mapper1.hpp"
#include "Bus.hpp"
#include "Cartridge.hpp"

Mapper1::Mapper1(Cartridge& cart) : Mapper(cart) {
    prgRam.resize(cart.prgRamSize(), 0);
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0);
    }
}

void Mapper1::applyMirroring() {
    if (!bus) return;
    // MMC1 control bits 0–1
    switch (control & 0x03) {
    case 0: bus->setMirror(PPU::Mirror::SingleLow); break;
    case 1: bus->setMirror(PPU::Mirror::SingleHigh); break;
    case 2: bus->setMirror(PPU::Mirror::Vertical); break;
    default: bus->setMirror(PPU::Mirror::Horizontal); break;
    }
}

void Mapper1::commitRegister(uint16_t addr, uint8_t value) {
    switch (addr & 0xE000) {
    case 0x8000:
        control = value;
        applyMirroring();
        break;
    case 0xA000:
        chrBank0 = value;
        break;
    case 0xC000:
        chrBank1 = value;
        break;
    default:
        prgBank = value;
        break;
    }
}

uint8_t Mapper1::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) {
        return 0x00;
    }
    if (addr < 0x8000) {
        return prgRam[addr - 0x6000];
    }

    const auto& prg = cart.prgRom();
    const size_t banks16 = prg.size() / 16384;
    if (banks16 == 0) return 0x00;
    const int mode = (control >> 2) & 0x03;
    size_t bank16 = 0;

    switch (mode) {
    case 0: // 32KB switchable (bit 0 of bank number ignored)
    case 1:
        bank16 = (prgBank & 0x0E) + ((addr >= 0xC000) ? 1u : 0u);
        break;
    case 2: // fixed first at $8000, 16KB switchable at $C000
        bank16 = (addr >= 0xC000) ? (prgBank & 0x0F) : 0;
        break;
    default: // mode 3: 16KB switchable at $8000, fixed last at $C000
        bank16 = (addr >= 0xC000) ? banks16 - 1 : (prgBank & 0x0F);
        break;
    }
    bank16 %= banks16;
    return prg[bank16 * 16384 + (addr & 0x3FFF)];
}

void Mapper1::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRam[addr - 0x6000] = data;
        return;
    }
    if (addr < 0x8000) return;

    // Bit 7 set: reset the shift register and force PRG mode 3
    if (data & 0x80) {
        shift = 0;
        writeCount = 0;
        control |= 0x0C;
        return;
    }

    // Five serial writes, LSB first, land in shift bits 0–4
    shift = static_cast<uint8_t>(shift | ((data & 1) << writeCount));
    if (++writeCount == 5) {
        commitRegister(addr, shift);
        shift = 0;
        writeCount = 0;
    }
}

size_t Mapper1::chrOffset(uint16_t addr) const {
    if (control & 0x10) {
        // 4KB mode: R0 at $0000, R1 at $1000
        const uint8_t bank = (addr < 0x1000) ? chrBank0 : chrBank1;
        return static_cast<size_t>(bank) * 4096 + (addr & 0x0FFF);
    }
    // 8KB mode: R0 covers $0000-$1FFF (R1 ignored, bit 0 of R0 ignored)
    return static_cast<size_t>(chrBank0 & 0x1E) * 4096 + (addr & 0x1FFF);
}

uint8_t Mapper1::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    const auto& chr = cart.chrRom();
    const uint8_t* image = chr.empty() ? chrRam.data() : chr.data();
    const size_t size = chr.empty() ? chrRam.size() : chr.size();
    if (size == 0) return 0x00;
    return image[chrOffset(addr) % size];
}

void Mapper1::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty() && !chrRam.empty()) {
        // CHR RAM writes follow the same banking as reads
        chrRam[chrOffset(addr) % chrRam.size()] = data;
    }
}

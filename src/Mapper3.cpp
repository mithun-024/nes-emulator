#include "Mapper3.hpp"
#include "Bus.hpp"
#include "Cartridge.hpp"

Mapper3::Mapper3(Cartridge& cart) : Mapper(cart) {
    prgRam.resize(cart.prgRamSize(), 0);
    if (cart.chrRom().empty()) {
        chrRam.resize(8192, 0);
    }
}

uint8_t Mapper3::prgBankAt(int slot) const {
    const auto& prg = cart.prgRom();
    const size_t banks8 = prg.size() / 8192;
    if (banks8 == 0) return 0;

    // slot: 0=$8000 1=$A000 2=$C000 3=$E000 (8KB units)
    size_t bank;
    const bool mode1 = (bankSelect & 0x40) != 0;
    if (!mode1) {
        // $8000=R6, $A000=R7, $C000=second-last, $E000=last
        switch (slot) {
        case 0: bank = regs[6] & 0x3F; break;
        case 1: bank = regs[7] & 0x3F; break;
        case 2: bank = banks8 - 2; break;
        default: bank = banks8 - 1; break;
        }
    } else {
        // $8000=second-last, $A000=R6, $C000=R7, $E000=last
        switch (slot) {
        case 0: bank = banks8 - 2; break;
        case 1: bank = regs[6] & 0x3F; break;
        case 2: bank = regs[7] & 0x3F; break;
        default: bank = banks8 - 1; break;
        }
    }
    return static_cast<uint8_t>(bank % banks8);
}

uint8_t Mapper3::chrBankAt(uint16_t addr) const {
    const auto& chr = cart.chrRom();
    const size_t banks1 =
        chr.empty() ? (chrRam.size() / 1024) : (chr.size() / 1024);
    if (banks1 == 0) return 0;

    // R0/R1 are 2KB banks (bit 0 ignored); R2–R5 are 1KB
    size_t bank;
    const bool mode1 = (bankSelect & 0x80) != 0;
    if (!mode1) {
        // $0000=R0(2K) $0800=R1(2K) $1000..$1C00=R2..R5(1K)
        if (addr < 0x0800) bank = (regs[0] & 0xFE) + ((addr & 0x7FF) >> 10);
        else if (addr < 0x1000) bank = (regs[1] & 0xFE) + ((addr & 0x7FF) >> 10);
        else bank = regs[2 + ((addr - 0x1000) >> 10)];
    } else {
        // $0000..$0C00=R2..R5(1K) $1000=R0(2K) $1800=R1(2K)
        if (addr < 0x1000) bank = regs[2 + (addr >> 10)];
        else if (addr < 0x1800) bank = (regs[0] & 0xFE) + (((addr - 0x1000) & 0x7FF) >> 10);
        else bank = (regs[1] & 0xFE) + (((addr - 0x1800) & 0x7FF) >> 10);
    }
    return static_cast<uint8_t>(bank % banks1);
}

uint8_t Mapper3::cpuRead(uint16_t addr) const {
    if (addr < 0x6000) return 0x00;
    if (addr < 0x8000) {
        return prgRamEnabled ? prgRam[addr - 0x6000] : 0x00;
    }
    const auto& prg = cart.prgRom();
    const size_t offset = static_cast<size_t>(prgBankAt((addr - 0x8000) >> 13)) * 8192 +
                          (addr & 0x1FFF);
    return prg[offset % prg.size()];
}

void Mapper3::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        if (prgRamEnabled) prgRam[addr - 0x6000] = data;
        return;
    }
    if (addr < 0x8000) return;

    switch (addr & 0xE000) {
    case 0x8000:
        if (addr & 1) {
            regs[bankSelect & 7] = data;
        } else {
            bankSelect = data;
        }
        break;
    case 0xA000:
        if (addr & 1) {
            prgRamEnabled = (data & 0x80) != 0;
        } else if (bus) {
            // 0 = vertical mirroring, 1 = horizontal
            bus->setMirror((data & 1) ? PPU::Mirror::Horizontal
                                      : PPU::Mirror::Vertical);
        }
        break;
    case 0xC000:
        if (addr & 1) {
            irqReload = true;
        } else {
            irqLatch = data;
        }
        break;
    default: // $E000–$FFFF
        if (addr & 1) {
            irqEnable = true;
        } else {
            irqEnable = false;
            irqLine = false;
        }
        break;
    }
}

uint8_t Mapper3::ppuRead(uint16_t addr) const {
    addr &= 0x1FFF;
    const auto& chr = cart.chrRom();
    if (!chr.empty()) {
        return chr[(chrBankAt(addr) * 1024 + (addr & 0x3FF)) % chr.size()];
    }
    return chrRam[(chrBankAt(addr) * 1024 + (addr & 0x3FF)) % chrRam.size()];
}

void Mapper3::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x1FFF;
    if (cart.chrRom().empty()) {
        chrRam[(chrBankAt(addr) * 1024 + (addr & 0x3FF)) % chrRam.size()] = data;
    }
}

void Mapper3::scanlineTick() {
    if (irqCounter == 0 || irqReload) {
        irqCounter = irqLatch;
        irqReload = false;
    } else {
        --irqCounter;
    }
    if (irqCounter == 0 && irqEnable) {
        irqLine = true;
    }
}

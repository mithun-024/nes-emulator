#ifndef MAPPER3_HPP
#define MAPPER3_HPP

#include <vector>
#include "Mapper.hpp"

// Mapper 4 (MMC3 / TxROM): 8KB PRG / 1KB-or-2KB CHR banking via two
// registers ($8000/$8001), mirroring + PRG RAM protect ($A000/$A001),
// and a scanline IRQ counter ($C000–$DFFF latch/reload, $E000/$E001
// disable/enable). The IRQ is clocked once per rendered scanline.
class Mapper3 : public Mapper {
public:
    explicit Mapper3(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

    void scanlineTick() override;
    bool irqPending() const override { return irqLine; }
    void irqAcknowledge() override { irqLine = false; }

private:
    uint8_t prgBankAt(int slot) const; // slot 0..3 → $8000,$A000,$C000,$E000
    uint8_t chrBankAt(uint16_t addr) const; // addr already0..0x1FFF

    std::vector<uint8_t> prgRam;
    std::vector<uint8_t> chrRam; // used only when the cart has no CHR ROM

    uint8_t bankSelect = 0; // $8000 (bit6 PRG mode, bit7 CHR mode)
    uint8_t regs[8] = {};
    bool prgRamEnabled = true;

    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    bool irqReload = false;
    bool irqEnable = false;
    bool irqLine = false;
};

#endif // MAPPER3_HPP

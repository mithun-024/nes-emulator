#ifndef MAPPER_HPP
#define MAPPER_HPP

#include <cstdint>

class Cartridge;
class Bus;

// Maps CPU/PPU cartridge address ranges onto PRG/CHR banks.
// Concrete mappers (0, MMC1, MMC3, …) override the virtuals.
class Mapper {
public:
    explicit Mapper(Cartridge& cart) : cart(cart) {}
    virtual ~Mapper() = default;

    // CPU bus: 0x4020–0xFFFF
    virtual uint8_t cpuRead(uint16_t addr) const = 0;
    virtual void cpuWrite(uint16_t addr, uint8_t data) = 0;

    // PPU bus: 0x0000–0x1FFF (CHR)
    virtual uint8_t ppuRead(uint16_t addr) const = 0;
    virtual void ppuWrite(uint16_t addr, uint8_t data) = 0;

    // Scanline clock for mappers with scanline IRQs (MMC3): called by the
    // PPU once per rendered scanline. Default: no-op.
    virtual void scanlineTick() {}

    // Level-triggered mapper IRQ line (checked by the System after each step)
    virtual bool irqPending() const { return false; }

    // CPU reads of $E000–$FFFF acknowledge the IRQ (MMC3)
    virtual void irqAcknowledge() {}

    // Set by Bus after construction so mappers can reach back
    // (mirroring changes, DMC reads, …)
    void setBus(Bus* b) { bus = b; }

protected:
    Cartridge& cart;
    Bus* bus = nullptr;
};

#endif // MAPPER_HPP

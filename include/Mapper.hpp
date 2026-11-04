#ifndef MAPPER_HPP
#define MAPPER_HPP

#include <cstdint>

class Cartridge;

// Maps CPU/PPU cartridge address ranges onto PRG/CHR banks.
// Concrete mappers (0, later MMC1/MMC3, …) override the virtuals.
class Mapper {
public:
    explicit Mapper(Cartridge& cart) : cart(cart) {}
    virtual ~Mapper() = default;

    // CPU bus: 0x4020–0xFFFF
    virtual uint8_t cpuRead(uint16_t addr) const = 0;
    virtual void cpuWrite(uint16_t addr, uint8_t data) = 0;

    // PPU bus: 0x0000–0x1FFF (CHR) — used from Milestone 5
    virtual uint8_t ppuRead(uint16_t addr) const = 0;
    virtual void ppuWrite(uint16_t addr, uint8_t data) = 0;

protected:
    Cartridge& cart;
};

#endif // MAPPER_HPP

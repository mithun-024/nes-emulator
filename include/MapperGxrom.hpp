#ifndef MAPPERGXROM_HPP
#define MAPPERGXROM_HPP

#include <vector>
#include <cstdint>
#include "Mapper.hpp"

// Mapper 66 (GxROM / Taito): one register at $8000–$FFFF.
// Bits 4–5 select a 32KB PRG bank, bits 0–1 select an 8KB CHR bank.
// No PRG RAM; mirroring comes from the cartridge header.
class MapperGxrom : public Mapper {
public:
    explicit MapperGxrom(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    uint8_t prgBank = 0;
    uint8_t chrBank = 0;
    std::vector<uint8_t> chrRam; // only when the cart has no CHR ROM
};

#endif // MAPPERGXROM_HPP

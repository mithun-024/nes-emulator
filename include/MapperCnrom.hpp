#ifndef MAPPERCNROM_HPP
#define MAPPERCNROM_HPP

#include <cstddef>
#include <cstdint>
#include "Mapper.hpp"

// Mapper 3 (CNROM): PRG is plain NROM (16KB mirrored or 32KB linear).
// A write to $8000–$FFFF selects an 8KB CHR bank (CHR is ROM, banks are
// switched in 8KB units across both pattern tables). No PRG RAM.
// Named MapperCnrom because "Mapper3" already refers to the MMC3 (mapper 4).
class MapperCnrom : public Mapper {
public:
    explicit MapperCnrom(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    uint8_t chrBank = 0;
};

#endif // MAPPERCNROM_HPP

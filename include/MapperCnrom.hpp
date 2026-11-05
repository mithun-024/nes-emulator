#ifndef MAPPERCNROM_HPP
#define MAPPERCNROM_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "Mapper.hpp"

// Mapper 3 (CNROM): PRG is plain NROM (16KB mirrored or 32KB linear).
// A write to $8000–$FFFF selects an 8KB CHR bank (CHR is ROM, banks are
// switched in 8KB units across both pattern tables).
// Named MapperCnrom because "Mapper3" already refers to the MMC3 (mapper 4).
// PRG RAM at $6000–$7FFF is emulated even though real CNROM boards omit it:
// dev/test carts rely on it to report results.
class MapperCnrom : public Mapper {
public:
    explicit MapperCnrom(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgRam;
    uint8_t chrBank = 0;
};

#endif // MAPPERCNROM_HPP

#ifndef MAPPERAXROM_HPP
#define MAPPERAXROM_HPP

#include <vector>
#include <cstdint>
#include "Mapper.hpp"

// Mapper 7 (AxROM): 32KB PRG banks switched as one block at $8000–$FFFF.
// Bit 4 of the write selects single-screen mirroring: 0 → nametable at
// $2000, 1 → nametable at $2400. CHR is 8KB ROM/RAM; no PRG RAM.
class MapperAxrom : public Mapper {
public:
    explicit MapperAxrom(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    size_t bankCount() const;

    uint8_t prgBank = 0;
    std::vector<uint8_t> chrRam;
};

#endif // MAPPERAXROM_HPP

#ifndef MAPPER1_HPP
#define MAPPER1_HPP

#include <vector>
#include "Mapper.hpp"

// Mapper 1 (MMC1 / SxROM): serial 5-bit shift register controls
// PRG/CHR banking and nametable mirroring.
//   $8000–$9FFF: control (mirroring, CHR/PRG mode)
//   $A000–$BFFF: CHR bank 0
//   $C000–$DFFF: CHR bank 1
//   $E000–$FFFF: PRG bank (+ PRG RAM enable in bit 4)
class Mapper1 : public Mapper {
public:
    explicit Mapper1(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    void commitRegister(uint16_t addr, uint8_t value);
    void applyMirroring();

    std::vector<uint8_t> prgRam;
    std::vector<uint8_t> chrRam; // used only when the cart has no CHR ROM

    uint8_t shift = 0;
    int writeCount = 0;
    uint8_t control = 0x0C; // PRG mode 3 after reset
    uint8_t chrBank0 = 0;
    uint8_t chrBank1 = 0;
    uint8_t prgBank = 0;
};

#endif // MAPPER1_HPP

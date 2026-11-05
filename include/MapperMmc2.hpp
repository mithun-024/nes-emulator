#ifndef MAPPERMMC2_HPP
#define MAPPERMMC2_HPP

#include <vector>
#include <cstdint>
#include "Mapper.hpp"

// Mapper 9 (MMC2 / PxROM): 8KB PRG banking plus four 4KB CHR banks chosen
// by two PPU "latches". Reading tile $FD or $FE from a pattern table loads
// the matching latch, and the CHR bank for that half of pattern memory is
// taken from the register programmed for that latch state.
//   $A000 → $0000 when latch 0 = $FD, $B000 → $0000 when latch 0 = $FE
//   $C000 → $1000 when latch 1 = $FD, $D000 → $1000 when latch 1 = $FE
//   $E000 → 8KB PRG bank at $8000 ($C000–$FFFF fixed to the last bank)
//   $F000 → mirroring (0 = vertical, 1 = horizontal)
class MapperMmc2 : public Mapper {
public:
    explicit MapperMmc2(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    size_t prgBanks8() const;
    size_t chrBanks4() const;
    uint8_t chrBankFor(uint16_t addr) const;
    void updateLatches(uint16_t addr) const;

    uint8_t prgBank = 0;
    uint8_t chrRegs[4] = {}; // $A000,$B000,$C000,$D000
    // Latches are updated by (const) PPU pattern fetches
    mutable uint8_t latch0 = 0xFD;
    mutable uint8_t latch1 = 0xFD;
    std::vector<uint8_t> chrRam;
};

#endif // MAPPERMMC2_HPP

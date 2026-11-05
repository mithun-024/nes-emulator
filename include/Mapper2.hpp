#ifndef MAPPER2_HPP
#define MAPPER2_HPP

#include <vector>
#include "Mapper.hpp"

// Mapper 2 (UxROM): 16KB PRG banks.
// $8000–$BFFF is switchable (any write selects the bank), $C000–$FFFF is
// fixed to the last bank. CHR is 8KB (ROM or RAM); there is no PRG RAM.
class Mapper2 : public Mapper {
public:
    explicit Mapper2(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    size_t bankCount() const;

    uint8_t prgBank = 0;
    std::vector<uint8_t> chrRam; // used only when the cart has no CHR ROM
};

#endif // MAPPER2_HPP

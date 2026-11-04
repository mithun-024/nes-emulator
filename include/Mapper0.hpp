#ifndef MAPPER0_HPP
#define MAPPER0_HPP

#include <vector>
#include "Mapper.hpp"

// Mapper 0 (NROM): no bank switching.
// 16KB PRG is mirrored at 0x8000 and 0xC000; 32KB fills 0x8000–0xFFFF.
// 8KB CHR at PPU 0x0000–0x1FFF (ROM, or RAM when the cart has no CHR ROM).
// 8KB PRG RAM at 0x6000–0x7FFF.
class Mapper0 : public Mapper {
public:
    explicit Mapper0(Cartridge& cart);

    uint8_t cpuRead(uint16_t addr) const override;
    void cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) const override;
    void ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgRam;
    std::vector<uint8_t> chrRam; // used only when the cart has no CHR ROM
};

#endif // MAPPER0_HPP

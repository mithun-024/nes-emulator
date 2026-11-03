#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>
#include <array>
#include "RAM.hpp"

class Bus {
public:
    Bus();
    ~Bus() = default;

    // CPU Bus read and write interface
    uint8_t cpuRead(uint16_t addr) const;
    void cpuWrite(uint16_t addr, uint8_t data);

    // Direct RAM access for testing/debugging
    RAM& getRAM() { return ram; }

private:
    RAM ram;

    // TEMPORARY placeholder for cartridge space (0x4020-0xFFFF).
    // Lets tests plant the reset vector at 0xFFFC before the real
    // iNES cartridge loader + Mapper 0 arrive in Milestone 4.
    std::array<uint8_t, 0x10000> cartSpace{};
};

#endif // BUS_HPP

#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>
#include <memory>
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
};

#endif // BUS_HPP

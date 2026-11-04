#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>
#include <array>
#include <memory>
#include "RAM.hpp"
#include "PPU.hpp"
#include "Cartridge.hpp"
#include "Mapper.hpp"
#include "Controller.hpp"

class Bus {
public:
    Bus();
    ~Bus() = default;

    // CPU Bus read and write interface
    uint8_t cpuRead(uint16_t addr) const;
    void cpuWrite(uint16_t addr, uint8_t data);

    // Insert an iNES cartridge; builds the mapper from the header id.
    // Returns false (and leaves the bus unchanged) for unsupported mappers.
    bool insertCartridge(std::unique_ptr<Cartridge> cart, std::string* error = nullptr);

    bool hasCartridge() const { return mapper != nullptr; }
    Cartridge* getCartridge() const { return cartridge.get(); }

    // Direct RAM / PPU access for testing/debugging
    RAM& getRAM() { return ram; }
    PPU& getPPU() { return ppu; }

    // Controller ports ($4016 = port 1, $4017 = port 2)
    Controller& port1() { return controllers[0]; }
    Controller& port2() { return controllers[1]; }

    // Extra CPU cycles stalled by OAM DMA ($4014); main loop consumes these
    int takeDmaStall() {
        const int s = dmaStall;
        dmaStall = 0;
        return s;
    }

private:
    RAM ram;
    PPU ppu;
    Controller controllers[2];

    std::unique_ptr<Cartridge> cartridge;
    std::unique_ptr<Mapper> mapper;
    int dmaStall = 0;

    // Fallback buffer for 0x4020–0xFFFF when no cartridge is inserted.
    // Lets unit tests plant a reset vector without loading a ROM.
    std::array<uint8_t, 0x10000> cartSpace{};
};

#endif // BUS_HPP

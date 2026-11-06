#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>
#include <array>
#include <memory>
#include "RAM.hpp"
#include "PPU.hpp"
#include "APU.hpp"
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

    // Advance the APU by N CPU cycles (generates audio samples)
    void tickApu(int cycles) { apu.tick(cycles); }
    APU& getAPU() { return apu; }

    // --- Cycle-level device sync ---
    // Every CPU bus access advances the PPU (3 dots) and APU (1 cycle) by one
    // CPU cycle *before* the access, so register reads/writes see device state
    // at the correct point inside the instruction. Internal (non-bus) cycles are
    // caught up by syncDevicesTo() at the end of each instruction.
    void advanceCycle() {
        ++devCycle;
        ppu.tick();
        ppu.tick();
        ppu.tick();
        apu.tick(1);
    }
    void syncDevicesTo(uint64_t cycle) {
        if (cycle <= devCycle) return;
        const uint64_t d = cycle - devCycle;
        devCycle = cycle;
        for (uint64_t i = 0; i < d * 3; ++i) ppu.tick();
        apu.tick(static_cast<int>(d));
    }
    // Rebase the watermark (CPU reset); device state itself is untouched.
    void setDeviceCycle(uint64_t cycle) { devCycle = cycle; }
    uint64_t deviceCycle() const { return devCycle; }

    // Level-triggered interrupt line: APU frame/DMC IRQ or mapper IRQ
    bool irqPending() const;

    // Change nametable mirroring at runtime (called by mappers)
    void setMirror(PPU::Mirror m) { ppu.setMirror(m); }

    // Current mapper (nullptr without a cartridge) — tests use this
    Mapper* getMapper() { return mapper.get(); }

    // Extra CPU cycles stolen by DMC/OAM DMA
    void addDmaStall(int cycles) { dmaStall += cycles; }

    // Extra CPU cycles stalled by OAM DMA ($4014); main loop consumes these
    int takeDmaStall() {
        const int s = dmaStall;
        dmaStall = 0;
        return s;
    }

    // CPU data bus open bus value
    uint8_t openBus() const { return cpuOpenBus; }

private:
    RAM ram;
    PPU ppu;
    APU apu;
    Controller controllers[2];

    std::unique_ptr<Cartridge> cartridge;
    std::unique_ptr<Mapper> mapper;
    int dmaStall = 0;
    uint64_t devCycle = 0; // CPU cycles devices have advanced through
    mutable uint8_t cpuOpenBus = 0;

    // Fallback buffer for 0x4020–0xFFFF when no cartridge is inserted.
    // Lets unit tests plant a reset vector without loading a ROM.
    std::array<uint8_t, 0x10000> cartSpace{};
};

#endif // BUS_HPP

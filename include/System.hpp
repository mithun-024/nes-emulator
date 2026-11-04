#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#include <cstdint>
#include "Bus.hpp"
#include "CPU.hpp"

// Ties the CPU and PPU together: 1 CPU cycle = 3 PPU dots, NMIs are
// serviced between instructions, and OAM DMA stalls are consumed.
class System {
public:
    System();

    Bus bus;
    CPU cpu;

    // Soft reset: restarts the CPU from the reset vector. RAM / CIRAM /
    // OAM / palette persist (matches a real console reset button).
    void reset();

    // Execute one CPU instruction and sync the PPU; returns CPU cycles consumed
    int step();

    // Run until the PPU finishes a frame (scanline 241, dot 1); returns CPU cycles
    uint64_t runFrame();

    // Run at least the given number of CPU cycles; returns cycles actually run
    uint64_t runCycles(uint64_t cycles);

private:
    void tickPpu(int cpuCycles);
};

#endif // SYSTEM_HPP

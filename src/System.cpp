#include "System.hpp"

System::System() : cpu(bus) {}

void System::reset() {
    cpu.reset();
}

void System::tickPpu(int cpuCycles) {
    PPU& ppu = bus.getPPU();
    for (int i = 0; i < cpuCycles * 3; ++i) {
        ppu.tick();
    }
}

int System::step() {
    const uint64_t before = cpu.total_cycles;
    cpu.step();
    int cycles = static_cast<int>(cpu.total_cycles - before);

    // OAM DMA ($4014) stalls the CPU while the PPU keeps running
    cycles += bus.takeDmaStall();
    tickPpu(cycles);
    bus.tickApu(cycles);

    // Service a pending NMI between instructions (7 more cycles, PPU keeps time)
    if (bus.getPPU().takeNmi()) {
        cpu.nmi();
        tickPpu(7);
        bus.tickApu(7);
        cycles += 7;
    } else if (bus.irqPending() && !cpu.getFlag(CPU::FLAG_I)) {
        // Level-triggered IRQ (APU frame/DMC or mapper); NMI has priority
        cpu.irq();
        tickPpu(7);
        bus.tickApu(7);
        cycles += 7;
    }
    return cycles;
}

uint64_t System::runFrame() {
    uint64_t total = 0;
    while (!bus.getPPU().takeFrameComplete()) {
        total += static_cast<uint64_t>(step());
    }
    return total;
}

uint64_t System::runCycles(uint64_t cycles) {
    uint64_t ran = 0;
    while (ran < cycles) {
        ran += static_cast<uint64_t>(step());
    }
    return ran;
}

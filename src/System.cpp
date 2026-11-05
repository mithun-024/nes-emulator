#include "System.hpp"

System::System() : cpu(bus) {}

void System::reset() {
    cpu.reset();
    nmiPending = false;
}

int System::step() {
    const uint64_t before = cpu.total_cycles;
    const bool iSetBefore = cpu.getFlag(CPU::FLAG_I);
    cpu.step();
    // Interrupt polling runs during the instruction's last cycle. CLI/SEI/PLP
    // rewrite I too late for that poll, so it still sees the old value and
    // the flag only affects interrupts from the next instruction onward.
    // RTI/BRK load I earlier, so their new value is visible to the poll.
    const bool irqAllowed =
        cpu.irqPollDelay ? !iSetBefore : !cpu.getFlag(CPU::FLAG_I);

    // OAM DMA ($4014) stalls the CPU while the devices keep time
    const int dma = bus.takeDmaStall();
    if (dma > 0) {
        cpu.total_cycles += static_cast<uint64_t>(dma);
        bus.syncDevicesTo(cpu.total_cycles);
    }

    // End-of-instruction NMI poll: consume the PPU's edge latch (visible
    // 2 dots after any line rise). The sample sees PPU-side rises made
    // during this instruction, but NOT the effect of this instruction's own
    // $2000 write — those are applied right after the poll (one-instruction
    // delay, blargg 04-nmi_control #11: immediate NMI occurs after the NEXT
    // instruction).
    PPU& ppu = bus.getPPU();
    if (ppu.takeNmiEdge()) {
        nmiPending = true;
    }
    ppu.applyDeferredNmi();

    int cycles = static_cast<int>(cpu.total_cycles - before);

    // Service a pending NMI between instructions (7 more cycles, devices keep time)
    if (nmiPending) {
        nmiPending = false;
        cpu.nmi();
        cycles += 7;
    } else if (irqAllowed && bus.irqPending()) {
        // Level-triggered IRQ (APU frame/DMC or mapper); NMI has priority
        cpu.irq();
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

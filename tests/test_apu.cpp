#include "../include/Bus.hpp"
#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void expect(bool cond, const char* name) {
    if (cond) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void testLengthCounterLoadAndStatus() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x01); // enable pulse 1
    bus.cpuWrite(0x4003, 0x00); // length index 0 → 10
    expect(apu.lengthCounter(0) == 10, "length counter loads 10 from table");
    expect((bus.cpuRead(0x4015) & 0x01) != 0, "$4015 bit0 set while length > 0");
}

static void testLengthRequiresChannelEnabled() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x00); // pulse 1 disabled
    bus.cpuWrite(0x4003, 0x00); // load attempt ignored
    expect(apu.lengthCounter(0) == 0, "load ignored while channel disabled");
}

static void testDisableClearsLength() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x01);
    bus.cpuWrite(0x4003, 0x00); // length 10
    bus.cpuWrite(0x4015, 0x00); // disable
    expect(apu.lengthCounter(0) == 0, "$4015 disable clears length counter");
    expect((bus.cpuRead(0x4015) & 0x01) == 0, "$4015 bit0 clear after disable");
}

static void testHalfFrameDecrementsLength() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x01);
    bus.cpuWrite(0x4000, 0x00); // halt off
    bus.cpuWrite(0x4003, 0x00); // length 10
    apu.tick(14913);            // one half-frame
    expect(apu.lengthCounter(0) == 9, "half-frame decrements length 10 → 9");
    apu.tick(29830 - 14913);    // second half-frame (29829 event)
    expect(apu.lengthCounter(0) == 8, "second half-frame decrements 9 → 8");
}

static void testLengthHalt() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x01);
    bus.cpuWrite(0x4000, 0x20); // halt/loop bit set
    bus.cpuWrite(0x4003, 0x00); // length 10
    apu.tick(14913 * 3);
    expect(apu.lengthCounter(0) == 10, "halt bit freezes length counter");
}

static void testFrameIrqAndAcknowledge() {
    Bus bus;
    APU& apu = bus.getAPU();

    expect(!apu.irqPending(), "no IRQ after reset");
    apu.tick(29829);
    expect(apu.frameIrqFlag(), "frame IRQ sets at cycle 29829 (4-step)");
    expect(apu.irqPending(), "irqPending reflects frame IRQ");
    expect((bus.cpuRead(0x4015) & 0x40) != 0, "$4015 read reports frame IRQ");
    expect(!apu.irqPending(), "reading $4015 acknowledges frame IRQ");
    expect((bus.cpuRead(0x4015) & 0x40) == 0, "frame IRQ stays cleared");
}

static void testFiveStepModeNoIrq() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4017, 0x80); // 5-step mode
    apu.tick(40000);
    expect(!apu.frameIrqFlag(), "5-step mode never raises frame IRQ");
}

static void testIrqInhibit() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4017, 0x40); // IRQ inhibit
    apu.tick(40000);
    expect(!apu.frameIrqFlag(), "inhibit bit suppresses frame IRQ");
}

static void testConstantVolumeOutput() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4015, 0x01);
    bus.cpuWrite(0x4000, 0xDF); // duty 3, constant volume, vol=15
    bus.cpuWrite(0x4002, 0x08); // period 8 (not muted)
    bus.cpuWrite(0x4003, 0x00); // length 10, resets duty position
    expect(apu.pulse1Output() == 15, "pulse 1 outputs constant volume 15");
    bus.cpuWrite(0x4015, 0x00);
    expect(apu.pulse1Output() == 0, "silent when length counter is 0");
}

static void testSampleGeneration() {
    Bus bus;
    APU& apu = bus.getAPU();

    // 44100 samples worth of CPU cycles (±1)
    apu.tick(44100 * 41);
    const auto samples = apu.drainSamples();
    expect(samples.size() >= 43000 && samples.size() <= 45000,
           "APU produces ~44100 samples per emulated second");
    expect(!samples.empty(), "drainSamples returns data");

    // Buffer resets after drain
    expect(apu.drainSamples().empty(), "drainSamples empties the buffer");
}

static void testDmcIrq() {
    Bus bus;
    APU& apu = bus.getAPU();

    bus.cpuWrite(0x4010, 0x80); // DMC IRQ enable, rate 0
    bus.cpuWrite(0x4012, 0x00); // address $C000
    bus.cpuWrite(0x4013, 0x00); // length 1 byte
    bus.cpuWrite(0x4015, 0x10); // enable DMC → starts a sample
    expect((bus.cpuRead(0x4015) & 0x10) != 0, "$4015 bit4 set while DMC plays");
    expect(!apu.irqPending(), "DMC IRQ not yet fired");

    apu.tick(1000); // enough for the 1-byte sample to finish
    expect((bus.cpuRead(0x4015) & 0x10) == 0, "DMC finishes 1-byte sample");
    expect((bus.cpuRead(0x4015) & 0x80) != 0, "DMC IRQ raised after sample ends");
    expect(apu.irqPending(), "DMC IRQ line stays asserted");
    bus.cpuWrite(0x4015, 0x00); // any $4015 write clears the DMC IRQ
    expect(!apu.irqPending(), "$4015 write clears DMC IRQ");
}

void run_apu_tests() {
    std::printf("\nRunning APU tests...\n");
    testLengthCounterLoadAndStatus();
    testLengthRequiresChannelEnabled();
    testDisableClearsLength();
    testHalfFrameDecrementsLength();
    testLengthHalt();
    testFrameIrqAndAcknowledge();
    testFiveStepModeNoIrq();
    testIrqInhibit();
    testConstantVolumeOutput();
    testSampleGeneration();
    testDmcIrq();
    if (failures == 0) {
        std::printf("APU tests passed!\n");
    } else {
        std::printf("APU tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}

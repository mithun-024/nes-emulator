#include "APU.hpp"
#include "Bus.hpp"
#include <algorithm>
#include <cmath>

namespace {

// Length counter load values (index = bits 7–3 of $4003/$4007/$400B/$400F)
constexpr uint8_t kLengthTable[32] = {
    10, 254, 20, 2,  40,  4,  80,  6,  160, 8,  60, 10, 14, 12, 26, 14,
    12, 16,  24, 18, 48,  20, 96,  22, 192, 24, 72, 26, 16, 28, 32, 30,
};

// 8-step duty cycles
constexpr uint8_t kDutyTable[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}, // 25% negated
};

// Noise timer periods (CPU cycles)
constexpr uint16_t kNoisePeriods[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

// DMC rate table (CPU cycles per output step)
constexpr uint16_t kDmcRates[16] = {
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106, 84,  72,  54,
};

// Triangle sequence: 15→0 then 0→15 (32 steps)
constexpr uint8_t kTriangleSeq[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

} // namespace

APU::APU(Bus* bus) : bus(bus) {
    reset();
}

void APU::reset() {
    pulse1 = Pulse{};
    pulse2 = Pulse{};
    triangle = Triangle{};
    noise = Noise{};
    dmc = Dmc{};
    fiveStep = false;
    irqInhibit = false;
    frameIRQ = false;
    frameCycle = 0;
    sampleAccum = 0.0;
    sampleBuffer.clear();
}

// ---------------------------------------------------------------- Pulse

int APU::Pulse::sweepTarget(int channel) const {
    const int change = timerPeriod >> sweepShift;
    if (sweepNegate) {
        // Pulse 1 uses one's complement (extra -1)
        return timerPeriod - change - (channel == 0 ? 1 : 0);
    }
    return timerPeriod + change;
}

void APU::Pulse::writeReg(int reg, uint8_t data, int channel) {
    switch (reg) {
    case 0:
        duty = data >> 6;
        loopLength = (data & 0x20) != 0;
        constantVolume = (data & 0x10) != 0;
        volumePeriod = data & 0x0F;
        envelopeStart = true;
        break;
    case 1:
        sweepEnabled = (data & 0x80) != 0;
        sweepPeriod = (data >> 4) & 0x07;
        sweepNegate = (data & 0x08) != 0;
        sweepShift = data & 0x07;
        sweepReload = true;
        break;
    case 2:
        timerPeriod = static_cast<uint16_t>((timerPeriod & 0x0700) | data);
        break;
    case 3:
        timerPeriod = static_cast<uint16_t>((timerPeriod & 0x00FF) | ((data & 0x07) << 8));
        dutyPos = 0;
        envelopeStart = true;
        if (enabled) {
            length = kLengthTable[data >> 3];
        }
        break;
    default:
        break;
    }
    (void)channel;
}

void APU::Pulse::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeVolume = 15;
        envelopeDivider = volumePeriod;
    } else if (envelopeDivider == 0) {
        envelopeDivider = volumePeriod;
        if (envelopeVolume > 0) {
            --envelopeVolume;
        } else if (loopLength) {
            envelopeVolume = 15;
        }
    } else {
        --envelopeDivider;
    }
}

void APU::Pulse::clockLength() {
    if (!loopLength && length > 0) {
        --length;
    }
}

void APU::Pulse::clockSweep(int channel) {
    const int target = sweepTarget(channel);
    if (sweepDivider == 0 && sweepEnabled && sweepShift > 0 &&
        timerPeriod >= 8 && target >= 0 && target <= 0x7FF) {
        timerPeriod = static_cast<uint16_t>(target);
    }
    if (sweepDivider == 0 || sweepReload) {
        sweepDivider = sweepPeriod;
        sweepReload = false;
    } else {
        --sweepDivider;
    }
}

void APU::Pulse::tickTimer() {
    if (timer > 0) {
        --timer;
        return;
    }
    timer = static_cast<uint16_t>(timerPeriod + 1);
    dutyPos = static_cast<uint8_t>((dutyPos + 1) & 7);
}

int APU::Pulse::output(int channel) const {
    if (!enabled || length == 0) return 0;
    if (timerPeriod < 8) return 0;
    const int target = sweepTarget(channel);
    if (target > 0x7FF) return 0;
    if (kDutyTable[duty][dutyPos] == 0) return 0;
    return constantVolume ? volumePeriod : envelopeVolume;
}

// ------------------------------------------------------------- Triangle

void APU::Triangle::writeReg(int reg, uint8_t data) {
    switch (reg) {
    case 0:
        control = (data & 0x80) != 0;
        linearReloadValue = data & 0x7F;
        break;
    case 2:
        timerPeriod = static_cast<uint16_t>((timerPeriod & 0x0700) | data);
        break;
    case 3:
        timerPeriod = static_cast<uint16_t>((timerPeriod & 0x00FF) | ((data & 0x07) << 8));
        linearReloadFlag = true;
        if (enabled) {
            length = kLengthTable[data >> 3];
        }
        break;
    default:
        break;
    }
}

void APU::Triangle::clockQuarter() {
    if (linearReloadFlag) {
        linearCounter = linearReloadValue;
    } else if (linearCounter > 0) {
        --linearCounter;
    }
    if (!control) {
        linearReloadFlag = false;
    }
}

void APU::Triangle::clockLength() {
    if (!control && length > 0) {
        --length;
    }
}

void APU::Triangle::tickTimer() {
    if (timer > 0) {
        --timer;
        return;
    }
    timer = static_cast<uint16_t>(timerPeriod);
    // Sequencer only advances while both counters allow it
    if (linearCounter > 0 && length > 0 && timerPeriod > 0) {
        seqPos = static_cast<uint8_t>((seqPos + 1) & 31);
    }
}

int APU::Triangle::output() const {
    if (!enabled || length == 0 || linearCounter == 0) return 0;
    return kTriangleSeq[seqPos];
}

// ---------------------------------------------------------------- Noise

void APU::Noise::writeReg(int reg, uint8_t data) {
    switch (reg) {
    case 0:
        loopLength = (data & 0x20) != 0;
        constantVolume = (data & 0x10) != 0;
        volumePeriod = data & 0x0F;
        envelopeStart = true;
        break;
    case 2:
        mode = (data & 0x80) != 0;
        periodIndex = data & 0x0F;
        timerPeriod = kNoisePeriods[periodIndex];
        break;
    case 3:
        envelopeStart = true;
        if (enabled) {
            length = kLengthTable[data >> 3];
        }
        break;
    default:
        break;
    }
}

void APU::Noise::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeVolume = 15;
        envelopeDivider = volumePeriod;
    } else if (envelopeDivider == 0) {
        envelopeDivider = volumePeriod;
        if (envelopeVolume > 0) {
            --envelopeVolume;
        } else if (loopLength) {
            envelopeVolume = 15;
        }
    } else {
        --envelopeDivider;
    }
}

void APU::Noise::clockLength() {
    if (!loopLength && length > 0) {
        --length;
    }
}

void APU::Noise::tickTimer() {
    if (timer > 0) {
        --timer;
        return;
    }
    timer = timerPeriod;
    const uint16_t feedback = (lfsr ^ (lfsr >> (mode ? 6 : 1))) & 1;
    lfsr = static_cast<uint16_t>((lfsr >> 1) | (feedback << 14));
}

int APU::Noise::output() const {
    if (!enabled || length == 0) return 0;
    if (lfsr & 1) return 0;
    return constantVolume ? volumePeriod : envelopeVolume;
}

// ------------------------------------------------------------------ DMC

void APU::Dmc::writeReg(int reg, uint8_t data) {
    switch (reg) {
    case 0:
        irqEnable = (data & 0x80) != 0;
        loop = (data & 0x40) != 0;
        rateIndex = data & 0x0F;
        if (!irqEnable) {
            irq = false;
        }
        break;
    case 1:
        directLoad = data & 0x7F;
        output = directLoad;
        break;
    case 2:
        sampleAddr = static_cast<uint16_t>(0xC000 + (data << 6));
        break;
    case 3:
        sampleLength = static_cast<uint16_t>((data << 4) + 1);
        break;
    default:
        break;
    }
}

void APU::Dmc::startSample() {
    currentAddr = sampleAddr;
    bytesRemaining = sampleLength;
}

void APU::Dmc::restartSample() {
    startSample();
    irq = false;
}

// ----------------------------------------------------------- APU logic

void APU::write(uint16_t addr, uint8_t data) {
    switch (addr) {
    case 0x4000: case 0x4001: case 0x4002: case 0x4003:
        pulse1.writeReg(addr & 3, data, 0);
        break;
    case 0x4004: case 0x4005: case 0x4006: case 0x4007:
        pulse2.writeReg(addr & 3, data, 1);
        break;
    case 0x4008: case 0x4009: case 0x400A: case 0x400B:
        triangle.writeReg(addr & 3, data);
        break;
    case 0x400C: case 0x400D: case 0x400E: case 0x400F:
        noise.writeReg(addr & 3, data);
        break;
    case 0x4010: case 0x4011: case 0x4012: case 0x4013:
        dmc.writeReg(addr & 3, data);
        break;
    case 0x4015:
        pulse1.enabled = (data & 0x01) != 0;
        if (!pulse1.enabled) pulse1.length = 0;
        pulse2.enabled = (data & 0x02) != 0;
        if (!pulse2.enabled) pulse2.length = 0;
        triangle.enabled = (data & 0x04) != 0;
        if (!triangle.enabled) triangle.length = 0;
        noise.enabled = (data & 0x08) != 0;
        if (!noise.enabled) noise.length = 0;
        if (data & 0x10) {
            if (dmc.bytesRemaining == 0) {
                dmc.restartSample();
            }
        } else {
            dmc.bytesRemaining = 0;
        }
        dmc.irq = false; // $4015 write clears the DMC IRQ
        break;
    case 0x4017:
        fiveStep = (data & 0x80) != 0;
        irqInhibit = (data & 0x40) != 0;
        if (irqInhibit) {
            frameIRQ = false;
        }
        frameCycle = 0;
        if (fiveStep) {
            // Immediate quarter+half clocks in 5-step mode
            clockQuarterFrame();
            clockHalfFrame();
        }
        break;
    default:
        break;
    }
}

uint8_t APU::readStatus() {
    uint8_t value = 0;
    if (pulse1.length > 0) value |= 0x01;
    if (pulse2.length > 0) value |= 0x02;
    if (triangle.length > 0) value |= 0x04;
    if (noise.length > 0) value |= 0x08;
    if (dmc.bytesRemaining > 0) value |= 0x10;
    if (frameIRQ) value |= 0x40;
    if (dmc.irq) value |= 0x80;
    frameIRQ = false; // reading $4015 acknowledges the frame IRQ
    return value;
}

void APU::clockQuarterFrame() {
    pulse1.clockEnvelope();
    pulse2.clockEnvelope();
    noise.clockEnvelope();
    triangle.clockQuarter();
}

void APU::clockHalfFrame() {
    pulse1.clockLength();
    pulse2.clockLength();
    pulse1.clockSweep(0);
    pulse2.clockSweep(1);
    noise.clockLength();
    triangle.clockLength();
}

void APU::runFrameCounter() {
    ++frameCycle;
    if (!fiveStep) {
        // 4-step mode: 29830 CPU cycles per frame
        if (frameCycle == 7457) {
            clockQuarterFrame();
        } else if (frameCycle == 14913) {
            clockQuarterFrame();
            clockHalfFrame();
        } else if (frameCycle == 22371) {
            clockQuarterFrame();
        } else if (frameCycle == 29829) {
            clockQuarterFrame();
            clockHalfFrame();
            if (!irqInhibit) {
                frameIRQ = true;
            }
        } else if (frameCycle >= 29830) {
            frameCycle = 0;
        }
    } else {
        // 5-step mode: 37282 CPU cycles per frame, no IRQ
        if (frameCycle == 7457) {
            clockQuarterFrame();
        } else if (frameCycle == 14913) {
            clockQuarterFrame();
            clockHalfFrame();
        } else if (frameCycle == 22371) {
            clockQuarterFrame();
        } else if (frameCycle == 37281) {
            clockQuarterFrame();
            clockHalfFrame();
        } else if (frameCycle >= 37282) {
            frameCycle = 0;
        }
    }
}

void APU::fillDmcBuffer() {
    if (dmc.bufferFull || dmc.bytesRemaining == 0 || bus == nullptr) {
        return;
    }
    dmc.sampleBuffer = bus->cpuRead(dmc.currentAddr);
    dmc.bufferFull = true;
    if (dmc.currentAddr == 0xFFFF) {
        dmc.currentAddr = 0x8000;
    } else {
        ++dmc.currentAddr;
    }
    if (--dmc.bytesRemaining == 0) {
        if (dmc.loop) {
            dmc.startSample();
        } else if (dmc.irqEnable) {
            dmc.irq = true;
        }
    }
    // DMC steals cycles from the CPU (approximated as a DMA stall)
    bus->addDmaStall(4);
}

void APU::tickDmc() {
    if (dmc.timer > 0) {
        --dmc.timer;
        return;
    }
    dmc.timer = kDmcRates[dmc.rateIndex];

    // Output unit: shift one delta bit
    if (!dmc.silence) {
        if (dmc.shiftReg & 1) {
            if (dmc.output <= 125) dmc.output = static_cast<uint8_t>(dmc.output + 2);
        } else {
            if (dmc.output >= 2) dmc.output = static_cast<uint8_t>(dmc.output - 2);
        }
    }
    dmc.shiftReg >>= 1;
    if (--dmc.bitsRemaining == 0) {
        dmc.bitsRemaining = 8;
        if (dmc.bufferFull) {
            dmc.shiftReg = dmc.sampleBuffer;
            dmc.bufferFull = false;
            dmc.silence = false;
        } else {
            dmc.silence = true;
        }
    }
    fillDmcBuffer();
}

void APU::clockPulseTimers() {
    pulse1.tickTimer();
    pulse2.tickTimer();
    triangle.tickTimer();
    noise.tickTimer();
    tickDmc();
}

double APU::mix() const {
    const int p = pulse1.output(0) + pulse2.output(1);
    double pulseOut = 0.0;
    if (p > 0) {
        pulseOut = 95.88 / (8128.0 / p + 100.0);
    }

    const int t = triangle.output() + noise.output() + dmc.output;
    double tndOut = 0.0;
    if (t > 0) {
        const double denom =
            triangle.output() / 8227.0 + noise.output() / 12241.0 + dmc.output / 22638.0;
        tndOut = 159.79 / (1.0 / denom + 100.0);
    }
    return pulseOut + tndOut;
}

void APU::tick(int cpuCycles) {
    for (int i = 0; i < cpuCycles; ++i) {
        runFrameCounter();
        clockPulseTimers();

        sampleAccum += 1.0;
        if (sampleAccum >= kCyclesPerSample) {
            sampleAccum -= kCyclesPerSample;
            const double v = mix();
            const double scaled = std::clamp(v * 60000.0, -32768.0, 32767.0);
            sampleBuffer.push_back(static_cast<int16_t>(std::lround(scaled)));
        }
    }
}

std::vector<int16_t> APU::drainSamples() {
    std::vector<int16_t> out;
    out.swap(sampleBuffer);
    return out;
}

// ------------------------------------------------------- test accessors

int APU::lengthCounter(int ch) const {
    switch (ch) {
    case 0: return pulse1.length;
    case 1: return pulse2.length;
    case 2: return triangle.length;
    case 3: return noise.length;
    default: return -1;
    }
}

int APU::pulse1Output() const { return pulse1.output(0); }
int APU::pulse2Output() const { return pulse2.output(1); }
int APU::triangleOutput() const { return triangle.output(); }
int APU::noiseOutput() const { return noise.output(); }
int APU::dmcOutput() const { return dmc.output; }

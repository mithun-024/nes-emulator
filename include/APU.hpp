#ifndef APU_HPP
#define APU_HPP

#include <cstdint>
#include <vector>

class Bus;

// 2A03 audio processing unit: 2 pulse channels, triangle, noise, DMC,
// frame counter. Ticked at the CPU rate (1.789773 MHz) and mixed down
// to 44.1 kHz mono int16 samples.
class APU {
public:
    explicit APU(Bus* bus = nullptr);

    void reset();
    void setBus(Bus* b) { bus = b; }

    // CPU register writes: $4000–$4013, $4015, $4017
    void write(uint16_t addr, uint8_t data);
    // $4015 status read — clears the frame IRQ flag
    uint8_t readStatus();

    // Advance by N CPU cycles (generates samples along the way)
    void tick(int cpuCycles);

    // Frame IRQ or DMC IRQ asserted?
    bool irqPending() const { return frameIRQ || dmc.irq; }

    // Hand generated samples over to the caller (audio backend)
    std::vector<int16_t> drainSamples();

    // --- Test / debug hooks ---
    int lengthCounter(int ch) const; // 0=pulse1 1=pulse2 2=triangle 3=noise
    int pulse1Output() const;
    int pulse2Output() const;
    int triangleOutput() const;
    int noiseOutput() const;
    int dmcOutput() const;
    int frameCycleCount() const { return frameCycle; }
    bool frameIrqFlag() const { return frameIRQ; }

private:
    // --- Shared helpers ---
    void clockQuarterFrame();
    void clockHalfFrame();
    void runFrameCounter();

    // --- Pulse channel (used twice) ---
    struct Pulse {
        // $4000/$4004
        uint8_t duty = 0;
        bool loopLength = false;
        bool constantVolume = false;
        uint8_t volumePeriod = 0;
        // $4001/$4005 sweep
        bool sweepEnabled = false;
        uint8_t sweepPeriod = 0;
        bool sweepNegate = false;
        uint8_t sweepShift = 0;
        bool sweepReload = false;
        uint8_t sweepDivider = 0;
        // $4002/$4006 timer
        uint16_t timerPeriod = 0;
        uint16_t timer = 0;
        uint8_t dutyPos = 0;
        // envelope
        bool envelopeStart = false;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeVolume = 0;
        // length counter
        uint8_t length = 0;
        bool enabled = false;

        int sweepTarget(int channel) const;
        void writeReg(int reg, uint8_t data, int channel);
        void clockEnvelope();
        void clockLength();
        void clockSweep(int channel);
        void tickTimer();
        int output(int channel) const;
    };

    struct Triangle {
        bool control = false; // $4008 bit 7 (length halt / linear reload)
        uint8_t linearReloadValue = 0;
        uint8_t linearCounter = 0;
        bool linearReloadFlag = false;
        uint16_t timerPeriod = 0;
        uint16_t timer = 0;
        uint8_t seqPos = 0;
        uint8_t length = 0;
        bool enabled = false;

        void writeReg(int reg, uint8_t data);
        void clockQuarter();
        void clockLength();
        void tickTimer();
        int output() const;
    };

    struct Noise {
        bool loopLength = false;
        bool constantVolume = false;
        uint8_t volumePeriod = 0;
        bool mode = false; // $400E bit 7: 9-bit vs 7-bit feedback
        uint8_t periodIndex = 0;
        uint16_t timerPeriod = 0;
        uint16_t timer = 0;
        uint16_t lfsr = 1;
        bool envelopeStart = false;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeVolume = 0;
        uint8_t length = 0;
        bool enabled = false;

        void writeReg(int reg, uint8_t data);
        void clockEnvelope();
        void clockLength();
        void tickTimer();
        int output() const;
    };

    struct Dmc {
        bool irqEnable = false;
        bool irq = false;
        bool loop = false;
        uint8_t rateIndex = 0;
        uint8_t directLoad = 0;
        uint16_t sampleAddr = 0;
        uint16_t sampleLength = 0;

        uint16_t currentAddr = 0;
        uint16_t bytesRemaining = 0;
        uint8_t sampleBuffer = 0;
        bool bufferFull = false;
        uint8_t shiftReg = 0;
        uint8_t bitsRemaining = 8;
        bool silence = true;
        uint16_t timer = 0;
        uint8_t output = 0;

        void writeReg(int reg, uint8_t data);
        void startSample();
        void restartSample();
    };

    void tickDmc();
    void fillDmcBuffer();
    void clockPulseTimers();
    double mix() const;

    Bus* bus = nullptr;

    Pulse pulse1{};
    Pulse pulse2{};
    Triangle triangle{};
    Noise noise{};
    Dmc dmc{};

    // Frame counter ($4017)
    bool fiveStep = false;
    bool irqInhibit = false;
    bool frameIRQ = false;
    int frameCycle = 0;

    // Sample generation (44100 Hz)
    double sampleAccum = 0.0;
    std::vector<int16_t> sampleBuffer;

    static constexpr double kCpuHz = 1789773.0;
    static constexpr double kSampleRate = 44100.0;
    static constexpr double kCyclesPerSample = kCpuHz / kSampleRate;
};

#endif // APU_HPP

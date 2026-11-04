#include "../include/Bus.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

static int failures = 0;

static void expect(bool cond, const char* name) {
    if (cond) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
}

// 128KB PRG (16 × 8KB) filled with its 8KB bank index,
// 32KB CHR (32 × 1KB) filled with its 1KB bank index, mapper 4.
static std::unique_ptr<Cartridge> makeMmc3Cart() {
    std::vector<uint8_t> img(16 + 131072 + 32768, 0);
    img[0] = 'N'; img[1] = 'E'; img[2] = 'S'; img[3] = 0x1A;
    img[4] = 8;   // 128KB PRG
    img[5] = 4;   // 32KB CHR
    img[6] = (4 << 4) & 0xF0;
    img[7] = 4 & 0xF0;
    for (size_t i = 0; i < 131072; i++) {
        img[16 + i] = static_cast<uint8_t>(i / 8192);
    }
    for (size_t i = 0; i < 32768; i++) {
        img[16 + 131072 + i] = static_cast<uint8_t>(i / 1024);
    }

    const char* path = "/tmp/nes_mmc3.nes";
    FILE* f = std::fopen(path, "wb");
    if (!f) return nullptr;
    std::fwrite(img.data(), 1, img.size(), f);
    std::fclose(f);

    std::string err;
    auto cart = Cartridge::load(path, &err);
    if (!cart) {
        std::printf("[FAIL] MMC3 fixture load: %s\n", err.c_str());
        ++failures;
    }
    return cart;
}

static void newMmc3Bus(Bus& bus) {
    std::string err;
    auto cart = makeMmc3Cart();
    if (!cart || !bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] MMC3 insertCartridge: %s\n", err.c_str());
        ++failures;
    }
}

// $8000 even = bank select, $8001 odd = data for the selected register
static void selectReg(Bus& bus, int reg, uint8_t value) {
    bus.cpuWrite(0x8000, static_cast<uint8_t>(reg));
    bus.cpuWrite(0x8001, value);
}

static void testPrgMode0() {
    Bus bus; newMmc3Bus(bus);
    // mode 0: $8000=R6, $A000=R7, $C000=second-last, $E000=last
    selectReg(bus, 6, 5);
    selectReg(bus, 7, 3);
    expect(bus.cpuRead(0x8000) == 5, "PRG mode 0: $8000 = R6");
    expect(bus.cpuRead(0xA000) == 3, "PRG mode 0: $A000 = R7");
    expect(bus.cpuRead(0xC000) == 14, "PRG mode 0: $C000 = second-last bank");
    expect(bus.cpuRead(0xE000) == 15, "PRG mode 0: $E000 = last bank");
}

static void testPrgMode1() {
    Bus bus; newMmc3Bus(bus);
    selectReg(bus, 6, 5);
    selectReg(bus, 7, 3);
    bus.cpuWrite(0x8000, 0x40); // PRG mode 1 (regs unchanged)
    expect(bus.cpuRead(0x8000) == 14, "PRG mode 1: $8000 = second-last bank");
    expect(bus.cpuRead(0xA000) == 5, "PRG mode 1: $A000 = R6");
    expect(bus.cpuRead(0xC000) == 3, "PRG mode 1: $C000 = R7");
    expect(bus.cpuRead(0xE000) == 15, "PRG mode 1: $E000 = last bank");
}

static void testPrgBankTopBitsIgnored() {
    Bus bus; newMmc3Bus(bus);
    // 16 banks total; R6/R7 ignore their top 2 bits (64 × 8KB max)
    selectReg(bus, 6, 0x15); // 21 → 21 % 16 = 5
    expect(bus.cpuRead(0x8000) == 5, "R6 masks to 6 bits (0x15 → bank 5)");
    selectReg(bus, 6, 0xFF);
    expect(bus.cpuRead(0x8000) == 15, "R6 masks to 6 bits (0xFF → bank 15)");
}

static void testChrMode0() {
    Bus bus; newMmc3Bus(bus);
    PPU& ppu = bus.getPPU();
    selectReg(bus, 0, 4);  // 2KB at $0000 → 1KB banks 4,5
    selectReg(bus, 1, 7);  // 2KB at $0800 → 1KB banks 6,7
    selectReg(bus, 2, 10); // 1KB at $1000
    selectReg(bus, 3, 11);
    selectReg(bus, 4, 12);
    selectReg(bus, 5, 13);

    expect(ppu.ppuRead(0x0000) == 4, "CHR mode 0: $0000 = R0 base");
    expect(ppu.ppuRead(0x0400) == 5, "CHR mode 0: $0400 = R0 base + 1KB");
    expect(ppu.ppuRead(0x0800) == 6, "CHR mode 0: $0800 = R1 base");
    expect(ppu.ppuRead(0x0C00) == 7, "CHR mode 0: $0C00 = R1 base + 1KB");
    expect(ppu.ppuRead(0x1000) == 10, "CHR mode 0: $1000 = R2");
    expect(ppu.ppuRead(0x1400) == 11, "CHR mode 0: $1400 = R3");
    expect(ppu.ppuRead(0x1800) == 12, "CHR mode 0: $1800 = R4");
    expect(ppu.ppuRead(0x1C00) == 13, "CHR mode 0: $1C00 = R5");

    selectReg(bus, 0, 5); // bit 0 ignored → still banks 4,5
    expect(ppu.ppuRead(0x0000) == 4 && ppu.ppuRead(0x0400) == 5,
           "CHR mode 0: R0 bit 0 ignored (2KB align)");
}

static void testChrMode1() {
    Bus bus; newMmc3Bus(bus);
    PPU& ppu = bus.getPPU();
    selectReg(bus, 0, 4);
    selectReg(bus, 1, 7);
    selectReg(bus, 2, 10);
    selectReg(bus, 3, 11);
    selectReg(bus, 4, 12);
    selectReg(bus, 5, 13);
    bus.cpuWrite(0x8000, 0x80); // CHR mode 1

    expect(ppu.ppuRead(0x0000) == 10, "CHR mode 1: $0000 = R2");
    expect(ppu.ppuRead(0x0400) == 11, "CHR mode 1: $0400 = R3");
    expect(ppu.ppuRead(0x0800) == 12, "CHR mode 1: $0800 = R4");
    expect(ppu.ppuRead(0x0C00) == 13, "CHR mode 1: $0C00 = R5");
    expect(ppu.ppuRead(0x1000) == 4, "CHR mode 1: $1000 = R0 base");
    expect(ppu.ppuRead(0x1400) == 5, "CHR mode 1: $1400 = R0 base + 1KB");
    expect(ppu.ppuRead(0x1800) == 6, "CHR mode 1: $1800 = R1 base");
    expect(ppu.ppuRead(0x1C00) == 7, "CHR mode 1: $1C00 = R1 base + 1KB");
}

static void testMirroringRegister() {
    Bus bus; newMmc3Bus(bus);
    PPU& ppu = bus.getPPU();
    bus.cpuWrite(0xA000, 0x00);
    expect(ppu.mirrorMode() == PPU::Mirror::Vertical,
           "$A000 bit0=0 → vertical mirroring");
    bus.cpuWrite(0xA000, 0x01);
    expect(ppu.mirrorMode() == PPU::Mirror::Horizontal,
           "$A000 bit0=1 → horizontal mirroring");
}

static void testIrqFiresAfterLatchClocks() {
    Bus bus; newMmc3Bus(bus);
    Mapper* mapper = bus.getMapper();
    expect(mapper != nullptr, "mapper present");

    bus.cpuWrite(0xC000, 5); // latch = 5
    bus.cpuWrite(0xC001, 0); // reload on next clock
    bus.cpuWrite(0xE001, 0); // enable IRQ

    for (int i = 0; i < 5; i++) mapper->scanlineTick();
    expect(!bus.irqPending(), "no IRQ before counter reaches 0");
    mapper->scanlineTick(); // 6th clock → counter = 0 → IRQ
    expect(bus.irqPending(), "IRQ fires when counter hits 0 (latch 5)");

    // Any CPU read of $E000–$FFFF acknowledges
    bus.cpuRead(0xE000);
    expect(!bus.irqPending(), "read of $E000 acknowledges IRQ");

    // Still enabled: another full countdown fires again
    bus.cpuWrite(0xC001, 0);
    for (int i = 0; i < 6; i++) mapper->scanlineTick();
    expect(bus.irqPending(), "IRQ re-fires after reload while enabled");
}

static void testIrqDisableAndAcknowledgeByWrite() {
    Bus bus; newMmc3Bus(bus);
    Mapper* mapper = bus.getMapper();

    bus.cpuWrite(0xC000, 1);
    bus.cpuWrite(0xC001, 0);
    bus.cpuWrite(0xE001, 0);
    mapper->scanlineTick();
    mapper->scanlineTick(); // latch 1 → reload then 0 → IRQ
    expect(bus.irqPending(), "IRQ raised with latch 1");

    bus.cpuWrite(0xE000, 0); // disable + acknowledge
    expect(!bus.irqPending(), "$E000 write disables and clears IRQ");

    bus.cpuWrite(0xC001, 0);
    mapper->scanlineTick();
    mapper->scanlineTick();
    expect(!bus.irqPending(), "disabled mapper never raises IRQ");
}

static void testIrqDisabledByDefault() {
    Bus bus; newMmc3Bus(bus);
    Mapper* mapper = bus.getMapper();

    bus.cpuWrite(0xC000, 0); // latch 0
    bus.cpuWrite(0xC001, 0); // reload
    for (int i = 0; i < 8; i++) mapper->scanlineTick();
    expect(!bus.irqPending(), "no IRQ while $E001 never written (disabled)");
}

static void testPrgRamProtect() {
    Bus bus; newMmc3Bus(bus);
    bus.cpuWrite(0xA001, 0x80); // enable PRG RAM
    bus.cpuWrite(0x6000, 0xAB);
    expect(bus.cpuRead(0x6000) == 0xAB, "PRG RAM enabled by $A001 bit7");
    bus.cpuWrite(0xA001, 0x00); // disable (reads return 0 too)
    bus.cpuWrite(0x6000, 0x00);
    bus.cpuWrite(0xA001, 0x80); // re-enable
    expect(bus.cpuRead(0x6000) == 0xAB, "disabled PRG RAM ignores writes");
}

void run_mmc3_tests() {
    std::printf("\nRunning MMC3 (mapper 4) tests...\n");
    testPrgMode0();
    testPrgMode1();
    testPrgBankTopBitsIgnored();
    testChrMode0();
    testChrMode1();
    testMirroringRegister();
    testIrqFiresAfterLatchClocks();
    testIrqDisableAndAcknowledgeByWrite();
    testIrqDisabledByDefault();
    testPrgRamProtect();
    if (failures == 0) {
        std::printf("MMC3 tests passed!\n");
    } else {
        std::printf("MMC3 tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}

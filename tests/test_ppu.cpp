#include "../include/Bus.hpp"
#include "../include/Cartridge.hpp"
#include "../include/PPU.hpp"
#include <cstdio>
#include <fstream>
#include <string>

static int failures = 0;

static void expect(bool cond, const char* name) {
    if (cond) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
}

// Minimal NROM cartridge: 16KB PRG (filled with pattern), 8KB CHR (zeros)
static std::unique_ptr<Cartridge> makeNromCart() {
    uint8_t bytes[16 + 16384 + 8192] = {};
    bytes[0] = 'N'; bytes[1] = 'E'; bytes[2] = 'S'; bytes[3] = 0x1A;
    bytes[4] = 1;  // 16KB PRG
    bytes[5] = 1;  // 8KB CHR
    bytes[6] = 0;  // mapper 0, horizontal mirroring
    bytes[7] = 0;
    // Fill PRG with a recognizable pattern
    for (int i = 0; i < 16384; ++i) {
        bytes[16 + i] = static_cast<uint8_t>(i & 0xFF);
    }

    std::string path = "/tmp/m5_nrom.nes";
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    f.close();

    std::string err;
    auto cart = Cartridge::load(path, &err);
    return cart;
}

static void testRegisterMirroring() {
    Bus bus;
    bus.cpuWrite(0x2000, 0x80);
    expect(bus.getPPU().ctrlRaw() == 0x80, "PPUCTRL write via $2000");
    bus.cpuWrite(0x2008, 0x00); // mirror of $2000
    expect(bus.getPPU().ctrlRaw() == 0x00, "PPUCTRL mirror $2008");

    bus.cpuWrite(0x2001, 0x1E);
    expect(bus.getPPU().maskRaw() == 0x1E, "PPUMASK write via $2001");
    bus.cpuWrite(0x23E9, 0x00); // $2001 mirrored throughout $2000–$3FFF
    expect(bus.getPPU().maskRaw() == 0x00, "PPUMASK mirror $23E9");
}

static void testStatusReadClearsVblankAndLatch() {
    Bus bus;
    bus.getPPU().setVblank();

    uint8_t st = bus.cpuRead(0x2002);
    expect((st & 0x80) != 0, "PPUSTATUS bit7 set after vblank");

    st = bus.cpuRead(0x2002);
    expect((st & 0x80) == 0, "PPUSTATUS bit7 cleared by read");

    // Latch reset: after $2002 read, next $2006 write is the high byte
    bus.cpuWrite(0x2006, 0x21); // would be low byte if latch were stale
    bus.cpuWrite(0x2006, 0x00);
    // v should be 0x2100 only if high-byte write was accepted first
    expect(bus.getPPU().vramAddrRaw() == 0x2100, "write latch reset by $2002 read");
}

static void testVramAddrAndData() {
    Bus bus;
    // Point PPUADDR at nametable $2000
    bus.cpuWrite(0x2006, 0x20);
    bus.cpuWrite(0x2006, 0x00);
    expect(bus.getPPU().vramAddrRaw() == 0x2000, "PPUADDR two-write sets v");

    bus.cpuWrite(0x2007, 0xAB);
    expect(bus.getPPU().vramAddrRaw() == 0x2001, "PPUDATA write increments v by 1");

    // Increment by 32 when PPUCTRL bit2 set
    bus.cpuWrite(0x2000, 0x04);
    bus.cpuWrite(0x2006, 0x20);
    bus.cpuWrite(0x2006, 0x10);
    bus.cpuWrite(0x2007, 0xCD);
    expect(bus.getPPU().vramAddrRaw() == 0x2030, "PPUDATA write increments v by 32");

    // Read back through buffer: first read is stale, second gets the value
    bus.cpuWrite(0x2006, 0x20);
    bus.cpuWrite(0x2006, 0x00);
    (void)bus.cpuRead(0x2007); // buffer fill (stale)
    uint8_t val = bus.cpuRead(0x2007);
    expect(val == 0xAB, "PPUDATA buffered read returns written nametable byte");
}

static void testPpuMemoryThroughBus() {
    Bus bus;
    // Write palette entry directly via $2007
    bus.cpuWrite(0x2006, 0x3F);
    bus.cpuWrite(0x2006, 0x00);
    bus.cpuWrite(0x2007, 0x2A);
    expect(bus.getPPU().ppuRead(0x3F00) == 0x2A, "palette write via $2007");

    // $3F10 mirrors $3F00
    expect(bus.getPPU().ppuRead(0x3F10) == 0x2A, "palette mirror $3F10 → $3F00");
}

static void testNametableMirroring() {
    // Horizontal: | A | A | B | B |
    {
        Bus bus;
        std::string err;
        auto cart = makeNromCart();
        expect(cart != nullptr, "load NROM for horizontal mirror test");
        if (!cart) return;
        expect(bus.insertCartridge(std::move(cart), &err), "insert horizontal NROM");

        bus.cpuWrite(0x2006, 0x20);
        bus.cpuWrite(0x2006, 0x00);
        bus.cpuWrite(0x2007, 0x11); // NT A

        // Under H-mirror |A|A|B|B|, $2400 aliases $2000 — do not write a
        // second value there; just verify the alias after writing NT B.

        bus.cpuWrite(0x2006, 0x28);
        bus.cpuWrite(0x2006, 0x00);
        bus.cpuWrite(0x2007, 0x33); // NT B

        expect(bus.getPPU().ppuRead(0x2000) == 0x11, "H-mirror: $2000 holds NT A");
        expect(bus.getPPU().ppuRead(0x2400) == 0x11, "H-mirror: $2400 aliases $2000");
        expect(bus.getPPU().ppuRead(0x2800) == 0x33, "H-mirror: $2800 is NT B");
        expect(bus.getPPU().ppuRead(0x2C00) == 0x33, "H-mirror: $2C00 aliases $2800");
    }

    // Vertical: | A | B | A | B |  (flags6 bit0 = 1)
    {
        uint8_t bytes[16 + 16384 + 8192] = {};
        bytes[0] = 'N'; bytes[1] = 'E'; bytes[2] = 'S'; bytes[3] = 0x1A;
        bytes[4] = 1; bytes[5] = 1;
        bytes[6] = 0x01; // vertical mirroring
        std::string path = "/tmp/m5_vert.nes";
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        f.close();

        Bus bus;
        std::string err;
        auto cart = Cartridge::load(path, &err);
        expect(cart != nullptr, "load NROM for vertical mirror test");
        if (!cart) return;
        expect(bus.insertCartridge(std::move(cart), &err), "insert vertical NROM");

        bus.cpuWrite(0x2006, 0x20);
        bus.cpuWrite(0x2006, 0x00);
        bus.cpuWrite(0x2007, 0x11); // NT A

        bus.cpuWrite(0x2006, 0x24);
        bus.cpuWrite(0x2006, 0x00);
        bus.cpuWrite(0x2007, 0x22); // NT B

        expect(bus.getPPU().ppuRead(0x2000) == 0x11, "V-mirror: $2000 is NT A");
        expect(bus.getPPU().ppuRead(0x2400) == 0x22, "V-mirror: $2400 is NT B");
        expect(bus.getPPU().ppuRead(0x2800) == 0x11, "V-mirror: $2800 aliases $2000");
        expect(bus.getPPU().ppuRead(0x2C00) == 0x22, "V-mirror: $2C00 aliases $2400");
    }
}

static void testOam() {
    Bus bus;
    bus.cpuWrite(0x2003, 0x10); // OAMADDR
    bus.cpuWrite(0x2004, 0x55);
    bus.cpuWrite(0x2004, 0x66); // OAMADDR auto-increments
    bus.cpuWrite(0x2003, 0x10);
    expect(bus.cpuRead(0x2004) == 0x55, "OAMDATA read at $2003 index");
    expect(bus.getPPU().oamRead(0x11) == 0x66, "OAM auto-increment on write");
}

static void testChrThroughMapper() {
    Bus bus;
    std::string err;
    auto cart = makeNromCart();
    if (!cart) return;
    expect(bus.insertCartridge(std::move(cart), &err), "insert NROM for CHR test");

    // PRG pattern was i & 0xFF; CHR was zeros — write CHR RAM? NROM with CHR size 1 is ROM.
    // Pattern table read returns 0 (ROM filled with zeros in fixture).
    expect(bus.getPPU().ppuRead(0x0000) == 0x00, "CHR ROM read via mapper");
}

void run_ppu_tests() {
    std::printf("Running PPU register tests...\n");
    testRegisterMirroring();
    testStatusReadClearsVblankAndLatch();
    testVramAddrAndData();
    testPpuMemoryThroughBus();
    testNametableMirroring();
    testOam();
    testChrThroughMapper();

    if (failures == 0) {
        std::printf("PPU register tests passed!\n");
    } else {
        std::printf("PPU register tests: %d FAILED\n", failures);
        std::exit(1);
    }
}

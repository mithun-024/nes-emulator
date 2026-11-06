#include "../include/Bus.hpp"
#include "../include/Cartridge.hpp"
#include "../include/PPU.hpp"
#include "../include/System.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
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

// Build a 16KB-PRG / 8KB-CHR NROM image and load it.
// prg must be <= 16384 bytes (placed at file offset 16), chr <= 8192.
static std::unique_ptr<Cartridge> makeCart(const std::vector<uint8_t>& prg,
                                           const std::vector<uint8_t>& chr) {
    std::vector<uint8_t> bytes(16 + 16384 + 8192, 0);
    bytes[0] = 'N'; bytes[1] = 'E'; bytes[2] = 'S'; bytes[3] = 0x1A;
    bytes[4] = 1; // 16KB PRG
    bytes[5] = 1; // 8KB CHR
    bytes[6] = 0; // mapper 0, horizontal mirroring
    for (size_t i = 0; i < prg.size() && i < 16384; ++i) {
        bytes[16 + i] = prg[i];
    }
    for (size_t i = 0; i < chr.size() && i < 8192; ++i) {
        bytes[16 + 16384 + i] = chr[i];
    }

    static int counter = 0;
    std::string path = "/tmp/m6_render_" + std::to_string(counter++) + ".nes";
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    f.close();

    std::string err;
    auto cart = Cartridge::load(path, &err);
    if (!cart) {
        std::printf("[FAIL] synthetic cart load: %s\n", err.c_str());
        ++failures;
    }
    return cart;
}

// 6502 program: enable NMI, spin; NMI handler increments $00 then RTI.
// Vectors: RESET -> $8000, NMI -> $8100.
static std::vector<uint8_t> nmiProgram() {
    std::vector<uint8_t> prg(16384, 0xEA); // NOP fill
    prg[0x0000] = 0xA9; prg[0x0001] = 0x80; // LDA #$80
    prg[0x0002] = 0x8D; prg[0x0003] = 0x00; prg[0x0004] = 0x20; // STA $2000
    prg[0x0005] = 0x4C; prg[0x0006] = 0x05; prg[0x0007] = 0x80; // JMP $8005
    prg[0x0100] = 0xE6; prg[0x0101] = 0x00; // INC $00
    prg[0x0102] = 0x40;                    // RTI
    prg[0x3FFA] = 0x00; prg[0x3FFB] = 0x81; // NMI   vector -> $8100
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80; // RESET vector -> $8000
    return prg;
}

// CHR with tile 0 = solid color 1 (lo plane $FF) and tile 1 = solid color 2 (hi plane $FF)
static std::vector<uint8_t> twoTileChr() {
    std::vector<uint8_t> chr(8192, 0);
    for (int row = 0; row < 8; ++row) {
        chr[0 * 16 + row] = 0xFF;        // tile 0 lo plane
        chr[1 * 16 + 8 + row] = 0xFF;    // tile 1 hi plane
    }
    return chr;
}

static void testBackgroundPixel() {
    System sys;
    auto cart = makeCart(std::vector<uint8_t>(16384, 0xEA), twoTileChr());
    if (!cart) return;
    std::string err;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insert: %s\n", err.c_str());
        ++failures;
        return;
    }

    // Tile 0 at nametable (0,0)
    sys.bus.cpuWrite(0x2006, 0x20);
    sys.bus.cpuWrite(0x2006, 0x00);
    sys.bus.cpuWrite(0x2007, 0x00);

    // Palette: backdrop black ($0F), color 1 = $21 (light blue)
    sys.bus.cpuWrite(0x2006, 0x3F);
    sys.bus.cpuWrite(0x2006, 0x00);
    sys.bus.cpuWrite(0x2007, 0x0F);
    sys.bus.cpuWrite(0x2007, 0x21);

    // BG on, show BG in leftmost 8px, pattern table 0, nametable 0, scroll 0
    sys.bus.cpuWrite(0x2000, 0x00);
    sys.bus.cpuWrite(0x2001, 0x0A);

    PPU& ppu = sys.bus.getPPU();
    for (int i = 0; i < 341 * 262; ++i) {
        ppu.tick();
    }
    expect(ppu.takeFrameComplete(), "full frame completes (241:1)");

    const uint32_t* fb = ppu.framebuffer();
    const uint32_t blue = 0xFF64B0FF;    // palette entry $21
    const uint32_t black = 0xFF000000;   // palette entry $0F
    expect(fb[0 * 256 + 0] == blue, "bg pixel (0,0) uses palette color 1");
    expect(fb[3 * 256 + 5] == blue, "bg pixel (5,3) uses palette color 1");
    sys.bus.cpuWrite(0x2001, 0x08); // BG on, clip left 8px
    for (int i = 0; i < 341 * 262; ++i) {
        ppu.tick();
    }
    (void)ppu.takeFrameComplete();
    expect(fb[0 * 256 + 0] == black, "left-clip hides BG in first 8 pixels");
    expect(fb[0 * 256 + 8] == blue, "BG visible again at x=8");
}

static void testSpritePixel() {
    System sys;
    auto cart = makeCart(std::vector<uint8_t>(16384, 0xEA), twoTileChr());
    if (!cart) return;
    std::string err;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insert: %s\n", err.c_str());
        ++failures;
        return;
    }

    // Sprite palette color 2 = $16 (red) at $3F12; backdrop black at $3F00
    sys.bus.cpuWrite(0x2006, 0x3F);
    sys.bus.cpuWrite(0x2006, 0x00);
    sys.bus.cpuWrite(0x2007, 0x0F);
    sys.bus.cpuWrite(0x2006, 0x3F);
    sys.bus.cpuWrite(0x2006, 0x12);
    sys.bus.cpuWrite(0x2007, 0x16);

    // Hide all sprites (y=$FF is off-screen), then place sprite 0
    for (int i = 0; i < 64; ++i) {
        sys.bus.getPPU().oamWrite(static_cast<uint8_t>(i * 4 + 0), 0xFF);
    }
    // OAM: sprite 0 at y=0, tile 1, attr 0, x=8
    sys.bus.getPPU().oamWrite(0, 0);
    sys.bus.getPPU().oamWrite(1, 1);
    sys.bus.getPPU().oamWrite(2, 0);
    sys.bus.getPPU().oamWrite(3, 8);

    // Sprites on (incl. left 8px), BG off
    sys.bus.cpuWrite(0x2000, 0x00);
    sys.bus.cpuWrite(0x2001, 0x14);

    PPU& ppu = sys.bus.getPPU();
    for (int i = 0; i < 341 * 262; ++i) {
        ppu.tick();
    }
    (void)ppu.takeFrameComplete();

    const uint32_t* fb = ppu.framebuffer();
    const uint32_t red = 0xFFB53120;     // palette entry $16
    const uint32_t black = 0xFF000000;   // backdrop $0F (palette[0] = 0)
    expect(fb[0 * 256 + 8] == red, "sprite pixel at (8,0)");
    expect(fb[7 * 256 + 15] == red, "sprite pixel at (15,7)");
    expect(fb[0 * 256 + 16] == black, "no sprite outside its 8px box");
    expect(fb[0 * 256 + 7] == black, "no sprite left of x=8");
}

static void testFrameTiming() {
    System sys;
    std::vector<uint8_t> prg(16384, 0xEA);
    prg[0x0000] = 0x4C; prg[0x0001] = 0x00; prg[0x0002] = 0x80; // JMP $8000
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80;                    // RESET -> $8000
    auto cart = makeCart(prg, std::vector<uint8_t>(8192, 0));
    if (!cart) return;
    std::string err;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insert: %s\n", err.c_str());
        ++failures;
        return;
    }
    sys.reset();

    // First "frame" starts mid-power-on sequence (sl 0) -> shorter than normal
    sys.runFrame();
    // A steady-state frame: 262 scanlines × 341 dots = 89342 dots = 29780.67 CPU cycles
    const uint64_t cycles = sys.runFrame();
    expect(cycles >= 29770 && cycles <= 29790, "steady-state frame ~29781 CPU cycles");
}

static void testNmiIntegration() {
    System sys;
    auto cart = makeCart(nmiProgram(), std::vector<uint8_t>(8192, 0));
    if (!cart) return;
    std::string err;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insert: %s\n", err.c_str());
        ++failures;
        return;
    }
    sys.reset();
    expect(sys.cpu.PC == 0x8000, "reset vector points at program");

    sys.runFrame();
    sys.runFrame();
    sys.runFrame();

    const uint8_t nmis = sys.bus.getRAM().read(0x0000);
    expect(nmis >= 2, "NMI handler ran at least twice in 3 frames");
    expect(sys.cpu.PC >= 0x8000, "CPU still executing after NMIs");
}

static void testOamDmaStallConsumedBySystem() {
    System sys;
    std::vector<uint8_t> prg(16384, 0xEA);
    prg[0x0000] = 0x4C; prg[0x0001] = 0x00; prg[0x0002] = 0x80;
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80;
    auto cart = makeCart(prg, std::vector<uint8_t>(8192, 0));
    if (!cart) return;
    std::string err;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insert: %s\n", err.c_str());
        ++failures;
        return;
    }
    sys.reset();

    // Plant a DMA source page and fire $4014 the way a game would
    for (int i = 0; i < 256; ++i) {
        sys.bus.cpuWrite(static_cast<uint16_t>(0x0200 + i), static_cast<uint8_t>(i));
    }
    sys.bus.cpuWrite(0x4014, 0x02);
    expect(sys.bus.takeDmaStall() == 513, "OAM DMA requests 513 stall cycles");

    // After System::step consumes it, OAM holds the copied page
    sys.bus.cpuWrite(0x4014, 0x02);
    sys.step();
    expect(sys.bus.getPPU().oamRead(0) == 0x00 &&
               sys.bus.getPPU().oamRead(255) == 0xFF,
           "DMA copied page $02 into OAM during step");
}

void run_render_tests() {
    std::printf("\nRunning render tests...\n");
    testBackgroundPixel();
    testSpritePixel();
    testFrameTiming();
    testNmiIntegration();
    testOamDmaStallConsumedBySystem();
    if (failures == 0) {
        std::printf("Render tests passed!\n");
    } else {
        std::printf("Render tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}

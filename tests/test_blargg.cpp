// Runs blargg's hardware test ROMs (https://github.com/christopherpow/nes-test-roms)
// through the normal System loop. Every modern blargg ROM speaks the same
// status protocol in PRG RAM: $6000 = $80 while running, then 0 on pass or a
// failure code; a NUL-terminated text report starts at $6004.
//
// The ROMs are NOT checked in — run tests/fetch_blargg.sh once to download
// them into tests/fixtures/nes-test-roms. Without them the suite skips.
//
// Results are compared against tests/blargg_baseline.txt: the run only fails
// when a ROM fails that isn't listed there (regression) or when a listed ROM
// starts passing (baseline is stale and must be trimmed). Accuracy work
// shrinks the baseline file over time.

#include "../include/System.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#ifndef NES_SOURCE_DIR
#define NES_SOURCE_DIR "."
#endif

namespace fs = std::filesystem;

static int ran = 0;

// Suite directories under tests/fixtures/nes-test-roms
static const char* kSuites[] = {
    "instr_test-v5/rom_singles",
    "ppu_vbl_nmi/rom_singles",
    "cpu_dummy_writes",
    "cpu_exec_space",
    "cpu_interrupts_v2",
    "instr_misc",
    "instr_timing",
    "apu_test",
    "apu_reset",
    "ppu_read_buffer",
    "ppu_open_bus",
    "oam_read",
    "oam_stress",
    // excluded: cpu_reset/* (needs a physical reset-button press mid-test)
    // excluded: blargg_nes_cpu_test5 (reports on screen only, superseded by
    //           instr_test-v5 which speaks the $6000 protocol)
    // excluded: cpu_dummy_reads (older framework: prints to the screen and
    //           loops forever at exit without ever writing $6000)
};

static std::string readReport(System& sys) {
    std::string text;
    for (uint16_t a = 0x6004; a < 0x6404; ++a) {
        const char c = static_cast<char>(sys.bus.cpuRead(a));
        if (c == '\0') break;
        text += c;
    }
    return text;
}

// Returns true when the ROM reported a passing result (status 0 after $80).
static bool runRom(const fs::path& rom, std::string* report, bool* timedOut) {
    *timedOut = false;
    std::string err;
    auto cart = Cartridge::load(rom.string(), &err);
    if (!cart) {
        *report = "load failed: " + err;
        return false;
    }

    System sys;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        *report = "insert failed: " + err;
        return false;
    }
    sys.reset();

    // Phase 1: wait for the test framework to announce it is running
    bool sawRunning = false;
    std::vector<uint16_t> pcSamples;
    for (int frame = 0; frame < 1200; ++frame) {
        sys.runFrame();
        if (frame % 200 == 0) pcSamples.push_back(sys.cpu.PC);
        if (sys.bus.cpuRead(0x6000) == 0x80) {
            sawRunning = true;
            break;
        }
    }
    if (!sawRunning) {
        char buf[320];
        int n = std::snprintf(
            buf, sizeof(buf),
            "never reported running ($6000=$%02X, PC now $%04X, PC@frames=",
            sys.bus.cpuRead(0x6000), sys.cpu.PC);
        for (size_t i = 0; i < pcSamples.size() && n < 300; ++i) {
            n += std::snprintf(buf + n, sizeof(buf) - n, "%s$%04X",
                               i ? "," : "", pcSamples[i]);
        }
        if (n < 300) std::snprintf(buf + n, sizeof(buf) - n, ")");
        *report = buf;
        *timedOut = true;
        return false;
    }

    // Phase 2: wait for the final result code
    for (int frame = 0; frame < 18000; ++frame) {
        sys.runFrame();
        const uint8_t status = sys.bus.cpuRead(0x6000);
        if (status != 0x80) {
            *report = readReport(sys);
            if (status != 0 && report->empty()) {
                *report = "result code " + std::to_string(status);
            }
            return status == 0;
        }
    }
    *report = "still running after 18000 frames";
    *timedOut = true;
    return false;
}

void run_blargg_tests() {
    const fs::path root =
        fs::path(NES_SOURCE_DIR) / "tests" / "fixtures" / "nes-test-roms";

    if (!fs::exists(root)) {
        std::printf("\nRunning blargg hardware tests...\n");
        std::printf("[SKIP] %s missing — run tests/fetch_blargg.sh first\n",
                    root.string().c_str());
        return;
    }

    std::printf("\nRunning blargg hardware tests...\n");
    const auto started = std::chrono::steady_clock::now();

    std::vector<std::string> failed;
    std::vector<std::string> reports;
    bool suiteMissing = false;

    for (const char* suite : kSuites) {
        const fs::path dir = root / suite;
        if (!fs::exists(dir)) {
            std::printf("[SKIP] suite %s not found\n", suite);
            suiteMissing = true;
            continue;
        }
        std::vector<fs::path> roms;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".nes") {
                roms.push_back(entry.path());
            }
        }
        std::sort(roms.begin(), roms.end());

        for (const auto& rom : roms) {
            std::string report;
            bool timedOut = false;
            const bool pass = runRom(rom, &report, &timedOut);
            ++ran;
            const std::string label =
                suite + std::string("/") + rom.filename().string();
            if (pass) {
                std::printf("[PASS] %s\n", label.c_str());
            } else {
                std::printf("[%s] %s: %s\n", timedOut ? "TIMEOUT" : "FAIL",
                            label.c_str(), report.c_str());
                failed.push_back(label);
            }
        }
    }

    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count();

    if (ran == 0) {
        std::printf("[FAIL] blargg suite found no ROMs\n");
        std::exit(1);
    }
    if (suiteMissing) {
        // Baseline entries for missing suites would look "fixed" — don't gate.
        std::printf("blargg: %d ROMs, %d failures, %lld ms (baseline skipped: "
                    "a suite directory is missing)\n",
                    ran, static_cast<int>(failed.size()),
                    static_cast<long long>(ms));
        return;
    }

    // Compare against the known-failure baseline
    const fs::path baselinePath =
        fs::path(NES_SOURCE_DIR) / "tests" / "blargg_baseline.txt";
    std::set<std::string> baseline;
    if (fs::exists(baselinePath)) {
        std::ifstream in(baselinePath);
        std::string line;
        while (std::getline(in, line)) {
            const auto hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            // trim
            const auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            const auto end = line.find_last_not_of(" \t\r\n");
            baseline.insert(line.substr(start, end - start + 1));
        }
    } else {
        std::printf("[FAIL] missing baseline %s\n", baselinePath.string().c_str());
        std::exit(1);
    }

    const std::set<std::string> actual(failed.begin(), failed.end());
    std::vector<std::string> regressions;
    std::vector<std::string> fixed;
    for (const auto& f : actual) {
        if (baseline.count(f) == 0) regressions.push_back(f);
    }
    for (const auto& b : baseline) {
        if (actual.count(b) == 0) fixed.push_back(b);
    }

    std::printf("blargg: %d ROMs, %d failing, %d in baseline, %lld ms\n", ran,
                static_cast<int>(actual.size()),
                static_cast<int>(baseline.size()),
                static_cast<long long>(ms));

    for (const auto& r : regressions) {
        std::printf("[REGRESSION] %s fails but is not in the baseline\n",
                    r.c_str());
    }
    for (const auto& f : fixed) {
        std::printf("[STALE BASELINE] %s passes now — remove it from "
                    "tests/blargg_baseline.txt\n",
                    f.c_str());
    }

    if (!regressions.empty() || !fixed.empty()) {
        std::printf("blargg tests: %d regressions, %d stale baseline entries\n",
                    static_cast<int>(regressions.size()),
                    static_cast<int>(fixed.size()));
        std::exit(1);
    }
    std::printf("blargg tests match the baseline!\n");
}

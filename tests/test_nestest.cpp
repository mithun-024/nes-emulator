#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "Bus.hpp"
#include "CPU.hpp"
#include "Cartridge.hpp"

#ifndef NES_SOURCE_DIR
#define NES_SOURCE_DIR "."
#endif

namespace {

struct LogState {
    uint16_t pc;
    uint8_t a, x, y, p, sp;
    uint64_t cyc;
};

// Parses one nestest.log line:
// C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
bool parseLogLine(const std::string& line, LogState& out) {
    if (line.size() < 10) return false;

    unsigned pc = 0;
    if (sscanf(line.c_str(), "%4x", &pc) != 1) return false;
    out.pc = static_cast<uint16_t>(pc);

    size_t pos = line.find(" A:");
    if (pos == std::string::npos) return false;

    unsigned a, x, y, p, sp;
    int matched = sscanf(line.c_str() + pos,
                         " A:%x X:%x Y:%x P:%x SP:%x",
                         &a, &x, &y, &p, &sp);
    if (matched != 5) return false;

    size_t cycPos = line.find("CYC:");
    if (cycPos == std::string::npos) return false;
    unsigned long long cyc = 0;
    try {
        cyc = std::stoull(line.substr(cycPos + 4));
    } catch (...) {
        return false;
    }

    out.a = static_cast<uint8_t>(a);
    out.x = static_cast<uint8_t>(x);
    out.y = static_cast<uint8_t>(y);
    out.p = static_cast<uint8_t>(p);
    out.sp = static_cast<uint8_t>(sp);
    out.cyc = static_cast<uint64_t>(cyc);
    return true;
}

} // namespace

void run_nestest() {
    std::cout << "Running nestest log comparison..." << std::endl;

    const std::string romPath = std::string(NES_SOURCE_DIR) + "/tests/fixtures/nestest.nes";
    const std::string logPath = std::string(NES_SOURCE_DIR) + "/tests/fixtures/nestest.log";

    Bus bus;
    CPU cpu(bus);

    std::string err;
    auto cart = Cartridge::load(romPath, &err);
    if (!cart) {
        std::cerr << "FATAL: " << err << std::endl;
        std::exit(1);
    }
    if (!bus.insertCartridge(std::move(cart), &err)) {
        std::cerr << "FATAL: " << err << std::endl;
        std::exit(1);
    }

    // Automated mode: execution starts at 0xC000 (not the reset vector)
    cpu.A = 0;
    cpu.X = 0;
    cpu.Y = 0;
    cpu.P = CPU::FLAG_U | CPU::FLAG_I; // 0x24
    cpu.SP = 0xFD;
    cpu.total_cycles = 7;              // log's first line is CYC:7
    cpu.PC = 0xC000;

    std::ifstream log(logPath);
    if (!log) {
        std::cerr << "FATAL: cannot open " << logPath << std::endl;
        std::exit(1);
    }

    std::string line;
    int lineNumber = 0;
    int matched = 0;

    while (std::getline(log, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lineNumber++;
        if (line.empty()) continue;

        LogState expected;
        if (!parseLogLine(line, expected)) {
            std::cerr << "[FAIL] unparseable log line " << lineNumber
                      << ": " << line << std::endl;
            std::exit(1);
        }

        auto fail = [&](const char* field, uint64_t got, uint64_t want) {
            std::cerr << "[FAIL] line " << lineNumber
                      << " opcode @ PC=" << std::hex
                      << " expected " << field << "=" << want
                      << " got=" << got << std::dec << "\n"
                      << "  log: " << line << "\n"
                      << "  cpu: PC=" << std::hex << cpu.PC
                      << " A=" << (int)cpu.A
                      << " X=" << (int)cpu.X
                      << " Y=" << (int)cpu.Y
                      << " P=" << (int)cpu.P
                      << " SP=" << (int)cpu.SP
                      << std::dec
                      << " CYC=" << cpu.total_cycles << std::endl;
            std::exit(1);
        };

        if (cpu.PC != expected.pc) fail("PC", cpu.PC, expected.pc);
        if (cpu.A  != expected.a)  fail("A",  cpu.A,  expected.a);
        if (cpu.X  != expected.x)  fail("X",  cpu.X,  expected.x);
        if (cpu.Y  != expected.y)  fail("Y",  cpu.Y,  expected.y);
        if (cpu.P  != expected.p)  fail("P",  cpu.P,  expected.p);
        if (cpu.SP != expected.sp) fail("SP", cpu.SP, expected.sp);
        if (cpu.total_cycles != expected.cyc)
            fail("CYC", cpu.total_cycles, expected.cyc);

        cpu.step();
        matched++;
    }

    std::cout << "[PASS] nestest: " << matched << " instructions matched" << std::endl;
}

// Entry point used by tests/main.cpp
void run_nestest_tests() {
    run_nestest();
}

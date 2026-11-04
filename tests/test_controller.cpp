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

// Pulse the strobe line high→low on $4016 (latches both ports)
static void strobe(Bus& bus) {
    bus.cpuWrite(0x4016, 1);
    bus.cpuWrite(0x4016, 0);
}

// Clock out 8 serial bits from a port
static uint8_t read8(Bus& bus, uint16_t addr) {
    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = static_cast<uint8_t>(value | ((bus.cpuRead(addr) & 1) << i));
    }
    return value;
}

static void testSerialOrder() {
    Bus bus;
    // A + Start pressed (bit0 and bit3)
    bus.port1().setMask((1u << Controller::BTN_A) | (1u << Controller::BTN_START));
    strobe(bus);
    const uint8_t bits = read8(bus, 0x4016);
    expect(bits == 0x09, "port1 shifts A..Right order (A+Start -> 0x09)");
}

static void testAllButtonsShiftOut() {
    Bus bus;
    bus.port1().setMask(0xFF);
    strobe(bus);
    expect(read8(bus, 0x4016) == 0xFF, "all buttons pressed -> eight 1s");

    Bus bus2;
    strobe(bus2);
    expect(read8(bus2, 0x4016) == 0x00, "no buttons -> eight 0s");
}

static void testReadsAfterEightReturnOne() {
    Bus bus;
    strobe(bus);
    (void)read8(bus, 0x4016);
    expect((bus.cpuRead(0x4016) & 1) == 1, "9th read returns 1");
    expect((bus.cpuRead(0x4016) & 1) == 1, "10th read returns 1");
}

static void testStrobeHeldHighReadsA() {
    Bus bus;
    bus.port1().setMask((1u << Controller::BTN_A) | (1u << Controller::BTN_B));
    bus.cpuWrite(0x4016, 1); // strobe stays high
    expect((bus.cpuRead(0x4016) & 1) == 1, "strobe high: read returns A");
    expect((bus.cpuRead(0x4016) & 1) == 1, "strobe high: repeated reads still A (no shift)");
    bus.port1().set(Controller::BTN_A, false);
    expect((bus.cpuRead(0x4016) & 1) == 0, "strobe high: live button state visible");
    bus.cpuWrite(0x4016, 0);
}

static void testPort2Independent() {
    Bus bus;
    bus.port2().setMask((1u << Controller::BTN_B) | (1u << Controller::BTN_RIGHT));
    strobe(bus); // $4016 strobe latches both ports
    expect(read8(bus, 0x4017) == 0x82, "port2 shifts B+Right -> 0x82");
    expect(read8(bus, 0x4016) == 0x00, "port1 unchanged -> 0x00");
}

static void testStrobeLatchesBothPorts() {
    Bus bus;
    bus.port1().setMask(1u << Controller::BTN_UP);
    bus.port2().setMask(1u << Controller::BTN_DOWN);
    strobe(bus);

    // Change buttons after the latch: shifted-out state must be frozen
    bus.port1().set(Controller::BTN_UP, false);
    bus.port2().set(Controller::BTN_DOWN, false);
    expect(read8(bus, 0x4016) == 0x10, "port1 latched Up survives release");
    expect(read8(bus, 0x4017) == 0x20, "port2 latched Down survives release");
}

static void testNewStrobeRelatches() {
    Bus bus;
    bus.port1().setMask(1u << Controller::BTN_LEFT);
    strobe(bus);
    (void)read8(bus, 0x4016); // drain one bit

    bus.port1().set(Controller::BTN_LEFT, false);
    bus.port1().set(Controller::BTN_SELECT, true);
    strobe(bus);
    expect(read8(bus, 0x4016) == 0x04, "second strobe latches new state (Select)");
}

void run_controller_tests() {
    std::printf("\nRunning controller tests...\n");
    testSerialOrder();
    testAllButtonsShiftOut();
    testReadsAfterEightReturnOne();
    testStrobeHeldHighReadsA();
    testPort2Independent();
    testStrobeLatchesBothPorts();
    testNewStrobeRelatches();
    if (failures == 0) {
        std::printf("Controller tests passed!\n");
    } else {
        std::printf("Controller tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}

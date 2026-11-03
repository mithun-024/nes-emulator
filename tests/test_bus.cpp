#include <iostream>
#include <cassert>
#include "Bus.hpp"

void testRAMMirroring() {
    Bus bus;

    // Test writing to 0x0000 and reading from mirrors (0x0800, 0x1000, 0x1800)
    bus.cpuWrite(0x0000, 0xAB);
    assert(bus.cpuRead(0x0000) == 0xAB);
    assert(bus.cpuRead(0x0800) == 0xAB);
    assert(bus.cpuRead(0x1000) == 0xAB);
    assert(bus.cpuRead(0x1800) == 0xAB);

    // Test writing to mirror address 0x0FFF and reading from base address 0x07FF
    bus.cpuWrite(0x0FFF, 0x55);
    assert(bus.cpuRead(0x07FF) == 0x55);
    assert(bus.cpuRead(0x0FFF) == 0x55);
    assert(bus.cpuRead(0x17FF) == 0x55);
    assert(bus.cpuRead(0x1FFF) == 0x55);

    std::cout << "[PASS] RAM mirroring test passed!" << std::endl;
}

int main() {
    std::cout << "Running Bus & RAM tests..." << std::endl;
    testRAMMirroring();
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}

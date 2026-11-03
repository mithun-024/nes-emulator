#include <iostream>
#include "Bus.hpp"

int main() {
    std::cout << "NES Emulator Skeleton Initialized!" << std::endl;
    Bus bus;
    
    // Quick demonstration of RAM read/write with mirroring
    bus.cpuWrite(0x0005, 0x42);
    std::cout << "Value written at 0x0005: 0x42" << std::endl;
    std::cout << "Value read from mirrored 0x0805: 0x" << std::hex << (int)bus.cpuRead(0x0805) << std::endl;

    return 0;
}

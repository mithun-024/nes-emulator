#include <iostream>

void run_bus_tests();
void run_cpu_tests();
void run_cartridge_tests();
void run_ppu_tests();
void run_render_tests();
void run_controller_tests();
void run_nestest_tests();

int main() {
    run_bus_tests();
    run_cpu_tests();
    run_cartridge_tests();
    run_ppu_tests();
    run_render_tests();
    run_controller_tests();
    run_nestest_tests();
    std::cout << "\nAll tests passed successfully!" << std::endl;
    return 0;
}

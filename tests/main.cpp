#include <iostream>

void run_bus_tests();
void run_cpu_tests();

int main() {
    run_bus_tests();
    run_cpu_tests();
    std::cout << "\nAll tests passed successfully!" << std::endl;
    return 0;
}

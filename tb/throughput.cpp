#include "utils/throughput_tool.hpp"
#define pci_bus 0x1a

int main() {
    // ============= CPU throughput benchmark ==============
    // cpu_throughput_h2c(pci_bus);
    // cpu_throughput_c2h(pci_bus);

    // ============= GPU throughput benchmark ==============
    gpu_throughput_h2c(pci_bus);
    gpu_throughput_c2h(pci_bus);
    return 0;
}
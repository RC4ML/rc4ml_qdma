#include "dma.hpp"
#define pci_bus 0x3e

int main() {
    h2c_benchmark(pci_bus);
    c2h_benchmark(pci_bus);
    // benchmark_bridge_write();
    return 0;
}

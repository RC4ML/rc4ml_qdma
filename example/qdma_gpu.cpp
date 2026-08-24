#include "dma.hpp"
#define pci_bus 0x3e

int main() {
    gpu_h2c_benchmark(pci_bus);
    return 0;
}
#include <QDMAController.h>
#include "dma.hpp"
#define pci_bus 0x3e

using namespace std;

int main() {
    h2c_benchmark_random(pci_bus);
    c2h_benchmark_random(pci_bus);
    concurrent_random(pci_bus);
    return 0;
}
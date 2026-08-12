#include <QDMAController.h>

#include "utils/random_tool.hpp"
#define pci_bus 0x1a

using namespace std;

int main() {
    // ============= CPU Random benchmark ==============
    // cpu_random_h2c(pci_bus);
    // cpu_random_c2h(pci_bus);
    // concurrent_random(pci_bus);

    // ============= GPU Random benchmark ==============
    gpu_random_h2c(pci_bus);
    gpu_random_c2h(pci_bus);

    return 0;
}
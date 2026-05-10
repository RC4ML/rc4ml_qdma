#include <QDMAController.h>

#include "utils/latency_tool.hpp"
#define pci_bus 0x1a

using namespace std;

int main() {
    // ============= CPU latency benchmark ==============
    // cpu_latency_h2c(pci_bus);
    // cpu_latency_c2h(pci_bus);
    // concurrent_latency(pci_bus);

    // ============= GPU latency benchmark ==============
    // gpu_latency_h2c(pci_bus);
    gpu_latency_c2h(pci_bus);
    
    return 0;
}
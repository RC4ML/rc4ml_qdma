#include <QDMAController.hpp>
#include "mmio.hpp"
#define pci_bus 0x3e

using namespace std;

int main()
{

    // benchmark_bridge_write(pci_bus, 0);
    benchmark_bridge_write(pci_bus, 1);

    return 0;
}
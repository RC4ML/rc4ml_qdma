#include <QDMAController.hpp>
#include "utils/mmio.hpp"
#define pci_bus 0x1a

using namespace std;

int main()
{

    // benchmark_bridge_write(pci_bus, 0);
    benchmark_bridge_write(pci_bus, 0);

    return 0;
}
#include <cstdint>

void cpu_random_h2c(uint8_t pci_bus);
void cpu_random_c2h(uint8_t pci_bus);

void gpu_random_h2c(uint8_t pci_bus);
void gpu_random_c2h(uint8_t pci_bus);
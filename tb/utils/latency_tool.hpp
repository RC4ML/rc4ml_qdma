#include <cstdint>

void cpu_latency_h2c(uint8_t pci_bus);
void cpu_latency_c2h(uint8_t pci_bus);

void gpu_latency_h2c(uint8_t pci_bus);
void gpu_latency_c2h(uint8_t pci_bus);
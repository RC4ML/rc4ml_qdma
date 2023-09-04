#ifndef _QDMACONTROLLER_H_
#define _QDMACONTROLLER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void init(uint8_t pci_bus, size_t bridge_bar_size);
void writeConfig(uint32_t index,uint32_t value,uint8_t pci_bus);
uint32_t readConfig(uint32_t index,uint8_t pci_bus);
void writeReg(uint32_t index,uint32_t value,uint8_t pci_bus);
uint32_t readReg(uint32_t index,uint8_t pci_bus);
void* qdma_alloc(size_t size, uint8_t pci_bus, bool print_addr);
void writeBridge(uint32_t index, uint64_t *value, uint8_t pci_bus);
void readBridge(uint32_t index, uint64_t *value, uint8_t pci_bus);
void* getBridgeAddr(uint8_t pci_bus);
void* getLiteAddr(uint8_t pci_bus);

void resetCounters(uint8_t pci_bus);
void printCounters(uint8_t pci_bus);

#ifdef __cplusplus
}
#endif

#endif
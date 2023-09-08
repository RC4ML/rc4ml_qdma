#ifndef mmio_hpp
#define mmio_hpp

#include <QDMAController.hpp>
#include <immintrin.h>
#include <iostream>
#include <pthread.h>

using namespace std;

void benchmark_bridge_write(uint8_t pci_bus);

#endif
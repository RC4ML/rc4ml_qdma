#ifndef mmio_hpp
#define mmio_hpp

#include <QDMAController.h>
#include <immintrin.h>
#include <iostream>
#include <pthread.h>

using namespace std;

void benchmark_bridge_write(uint8_t, uint8_t);

#endif
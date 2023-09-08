#include "dma.hpp"
#include "mmio.hpp"

int main() {
    h2c_benchmark(0x43);
    c2h_benchmark(0x43);
    benchmark_bridge_write(0x43);
    return 0;
}
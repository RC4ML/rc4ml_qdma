#include "dma.hpp"

int main() {
    h2c_benchmark(0x89);
    c2h_benchmark(0x89);
    // benchmark_bridge_write();
    return 0;
}

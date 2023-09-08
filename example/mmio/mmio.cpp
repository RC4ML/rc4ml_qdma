#include "mmio.hpp"

FPGACtl *fpga_ctl_global;

void *write_bridge_sub(void *args) {
    uint64_t data[8];
    for (int i = 0; i < 8; i++) {
        data[i] = 1;
    }
    size_t size = 4 * 1024 * 1024;

    for (int i = 0; i < size / 64; i += 1) {
        fpga_ctl_global->writeBridge(i % (size / 64), data);
    }
    return 0;
}

void benchmark_bridge_write(uint8_t pci_bus) {
    printf("=====AXIB throughput benchmark start=====\n");
    // FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);

    size_t size = 4 * 1024 * 1024;//byte
    int num_threads = 64;
    pthread_t tids[num_threads];
    fpga_ctl_global = fpga_ctl;

    //start timer
    struct timespec start_timer, end_timer;
    clock_gettime(CLOCK_MONOTONIC, &start_timer);

    for (int i = 0; i < num_threads; i++) {
        int ret = pthread_create(&tids[i], NULL, write_bridge_sub, NULL);
        if (ret != 0) {
            cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(tids[i], NULL);
    }

    //end timer
    clock_gettime(CLOCK_MONOTONIC, &end_timer);
    double time = (end_timer.tv_sec - start_timer.tv_sec) + 1.0 * (end_timer.tv_nsec - start_timer.tv_nsec) / 1e9;
    printf("time:%f s\n", time);
    printf("speed:%f GB/s\n", size * num_threads / time / 1024 / 1024 / 1024);
}
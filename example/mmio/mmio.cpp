#include "mmio.hpp"
#include <QDMAController.hpp>

void *write_bridge_sub(void *args)
{
    __m512i data;
    for (int i = 0; i < 8; i++)
    {
        data[i] = 1;
    }

    size_t addr, size, repeat_times;
    addr = ((size_t *)args)[0];
    size = ((size_t *)args)[1];
    repeat_times = ((size_t *)args)[2];

    for (size_t t = 0; t < repeat_times; t++)
    {
        for (size_t i = addr; i < addr + size; i += 512)
        {
            _mm512_stream_si512((__m512i *)(i + 64 * 0), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 1), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 2), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 3), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 4), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 5), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 6), data);
            _mm512_stream_si512((__m512i *)(i + 64 * 7), data);
        }
    }

    return 0;
}

void benchmark_bridge_write(uint8_t pci_bus, uint8_t is_wc)
{
    size_t size = 256 * 1024 * 1024; // size of data that a single thread should write (in bytes)
    int num_threads = 4, repeat_times = 16;
    pthread_t tids[num_threads];
    size_t args[num_threads][3];

    printf("MMIO Bridge Write Benchmark\n");

    FPGACtl::explictInit(pci_bus, size * num_threads, is_wc);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);
    void *bridge = (void *)(fpga_ctl->getBridgeAddr());
    printf("Bridge: %lx\n", (size_t)bridge);

    // initialize params
    for (int i = 0; i < num_threads; i++)
    {
        args[i][0] = (size_t)bridge + i * size;
        // args[i][0] = (size_t)bridge;
        args[i][1] = size;
        args[i][2] = (size_t)repeat_times;
    }

    printf("Starting %d threads, repeating %d times with writecombine %s.\n", num_threads, repeat_times, is_wc ? "enabled" : "disabled");

    // start timer
    struct timespec start_timer, end_timer;
    clock_gettime(CLOCK_MONOTONIC, &start_timer);

    for (int i = 0; i < num_threads; i++)
    {
        int ret = pthread_create(&tids[i], NULL, write_bridge_sub, args[i]);
        if (ret != 0)
        {
            cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(tids[i], NULL);
    }

    // end timer
    clock_gettime(CLOCK_MONOTONIC, &end_timer);
    double time = (end_timer.tv_sec - start_timer.tv_sec) + 1.0 * (end_timer.tv_nsec - start_timer.tv_nsec) / 1e9;
    printf("time: %f s\n", time);
    printf("speed: %f GB/s\n", size * num_threads * repeat_times / time / 1024 / 1024 / 1024);
}
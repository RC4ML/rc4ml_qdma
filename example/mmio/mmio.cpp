#include "mmio.hpp"
#include <QDMAController.hpp>

// #include <stdint.h>
// #include <stdlib.h>
// #include <string.h>
// #include <immintrin.h>

// #ifndef min
// #define min(A, B) ((A) < (B) ? (A) : (B))
// #endif

// int memcpy_uncached_store_avx(void *dest, const void *src, size_t n_bytes)
// {
//     int ret = 0;
// #ifdef __AVX__
//     char *d = (char *)dest;
//     uintptr_t d_int = (uintptr_t)d;
//     const char *s = (const char *)src;
//     uintptr_t s_int = (uintptr_t)s;
//     size_t n = n_bytes;

//     // align dest to 256-bits
//     if (d_int & 0x1f)
//     {
//         size_t nh = min(0x20 - (d_int & 0x1f), n);
//         memcpy(d, s, nh);
//         d += nh;
//         d_int += nh;
//         s += nh;
//         s_int += nh;
//         n -= nh;
//     }

//     if (s_int & 0x1f)
//     { // src is not aligned to 256-bits
//         __m256d r0, r1, r2, r3;
//         // unroll 4
//         while (n >= 4 * sizeof(__m256d))
//         {
//             r0 = _mm256_loadu_pd((double *)(s + 0 * sizeof(__m256d)));
//             r1 = _mm256_loadu_pd((double *)(s + 1 * sizeof(__m256d)));
//             r2 = _mm256_loadu_pd((double *)(s + 2 * sizeof(__m256d)));
//             r3 = _mm256_loadu_pd((double *)(s + 3 * sizeof(__m256d)));
//             _mm256_stream_pd((double *)(d + 0 * sizeof(__m256d)), r0);
//             _mm256_stream_pd((double *)(d + 1 * sizeof(__m256d)), r1);
//             _mm256_stream_pd((double *)(d + 2 * sizeof(__m256d)), r2);
//             _mm256_stream_pd((double *)(d + 3 * sizeof(__m256d)), r3);
//             s += 4 * sizeof(__m256d);
//             d += 4 * sizeof(__m256d);
//             n -= 4 * sizeof(__m256d);
//         }
//         while (n >= sizeof(__m256d))
//         {
//             r0 = _mm256_loadu_pd((double *)(s));
//             _mm256_stream_pd((double *)(d), r0);
//             s += sizeof(__m256d);
//             d += sizeof(__m256d);
//             n -= sizeof(__m256d);
//         }
//     }
//     else
//     { // or it IS aligned
//         __m256d r0, r1, r2, r3, r4, r5, r6, r7;
//         // unroll 8
//         while (n >= 8 * sizeof(__m256d))
//         {
//             r0 = _mm256_load_pd((double *)(s + 0 * sizeof(__m256d)));
//             r1 = _mm256_load_pd((double *)(s + 1 * sizeof(__m256d)));
//             r2 = _mm256_load_pd((double *)(s + 2 * sizeof(__m256d)));
//             r3 = _mm256_load_pd((double *)(s + 3 * sizeof(__m256d)));
//             r4 = _mm256_load_pd((double *)(s + 4 * sizeof(__m256d)));
//             r5 = _mm256_load_pd((double *)(s + 5 * sizeof(__m256d)));
//             r6 = _mm256_load_pd((double *)(s + 6 * sizeof(__m256d)));
//             r7 = _mm256_load_pd((double *)(s + 7 * sizeof(__m256d)));
//             _mm256_stream_pd((double *)(d + 0 * sizeof(__m256d)), r0);
//             _mm256_stream_pd((double *)(d + 1 * sizeof(__m256d)), r1);
//             _mm256_stream_pd((double *)(d + 2 * sizeof(__m256d)), r2);
//             _mm256_stream_pd((double *)(d + 3 * sizeof(__m256d)), r3);
//             _mm256_stream_pd((double *)(d + 4 * sizeof(__m256d)), r4);
//             _mm256_stream_pd((double *)(d + 5 * sizeof(__m256d)), r5);
//             _mm256_stream_pd((double *)(d + 6 * sizeof(__m256d)), r6);
//             _mm256_stream_pd((double *)(d + 7 * sizeof(__m256d)), r7);
//             s += 8 * sizeof(__m256d);
//             d += 8 * sizeof(__m256d);
//             n -= 8 * sizeof(__m256d);
//         }
//         while (n >= sizeof(__m256d))
//         {
//             r0 = _mm256_load_pd((double *)(s));
//             _mm256_stream_pd((double *)(d), r0);
//             s += sizeof(__m256d);
//             d += sizeof(__m256d);
//             n -= sizeof(__m256d);
//         }
//     }

//     if (n)
//         memcpy(d, s, n);

//     // fencing is needed even for plain memcpy(), due to performance
//     // being hit by delayed flushing of WC buffers
//     _mm_sfence();

// #else
// #error "this file should be compiled with -mavx"
// #endif
//     return ret;
// }

// int memcpy_cached_store_avx(void *dest, const void *src, size_t n_bytes)
// {
//     int ret = 0;
// #ifdef __AVX__
//     char *d = (char *)dest;
//     uintptr_t d_int = (uintptr_t)d;
//     const char *s = (const char *)src;
//     uintptr_t s_int = (uintptr_t)s;
//     size_t n = n_bytes;

//     // align dest to 256-bits
//     if (d_int & 0x1f)
//     {
//         size_t nh = min(0x20 - (d_int & 0x1f), n);
//         memcpy(d, s, nh);
//         d += nh;
//         d_int += nh;
//         s += nh;
//         s_int += nh;
//         n -= nh;
//     }

//     if (s_int & 0x1f)
//     { // src is not aligned to 256-bits
//         __m256d r0, r1, r2, r3;
//         // unroll 4
//         while (n >= 4 * sizeof(__m256d))
//         {
//             r0 = _mm256_loadu_pd((double *)(s + 0 * sizeof(__m256d)));
//             r1 = _mm256_loadu_pd((double *)(s + 1 * sizeof(__m256d)));
//             r2 = _mm256_loadu_pd((double *)(s + 2 * sizeof(__m256d)));
//             r3 = _mm256_loadu_pd((double *)(s + 3 * sizeof(__m256d)));
//             _mm256_store_pd((double *)(d + 0 * sizeof(__m256d)), r0);
//             _mm256_store_pd((double *)(d + 1 * sizeof(__m256d)), r1);
//             _mm256_store_pd((double *)(d + 2 * sizeof(__m256d)), r2);
//             _mm256_store_pd((double *)(d + 3 * sizeof(__m256d)), r3);
//             s += 4 * sizeof(__m256d);
//             d += 4 * sizeof(__m256d);
//             n -= 4 * sizeof(__m256d);
//         }
//         while (n >= sizeof(__m256d))
//         {
//             r0 = _mm256_loadu_pd((double *)(s));
//             _mm256_store_pd((double *)(d), r0);
//             s += sizeof(__m256d);
//             d += sizeof(__m256d);
//             n -= sizeof(__m256d);
//         }
//     }
//     else
//     { // or it IS aligned
//         __m256d r0, r1, r2, r3;
//         // unroll 4
//         while (n >= 4 * sizeof(__m256d))
//         {
//             r0 = _mm256_load_pd((double *)(s + 0 * sizeof(__m256d)));
//             r1 = _mm256_load_pd((double *)(s + 1 * sizeof(__m256d)));
//             r2 = _mm256_load_pd((double *)(s + 2 * sizeof(__m256d)));
//             r3 = _mm256_load_pd((double *)(s + 3 * sizeof(__m256d)));
//             _mm256_store_pd((double *)(d + 0 * sizeof(__m256d)), r0);
//             _mm256_store_pd((double *)(d + 1 * sizeof(__m256d)), r1);
//             _mm256_store_pd((double *)(d + 2 * sizeof(__m256d)), r2);
//             _mm256_store_pd((double *)(d + 3 * sizeof(__m256d)), r3);
//             s += 4 * sizeof(__m256d);
//             d += 4 * sizeof(__m256d);
//             n -= 4 * sizeof(__m256d);
//         }
//         while (n >= sizeof(__m256d))
//         {
//             r0 = _mm256_load_pd((double *)(s));
//             _mm256_store_pd((double *)(d), r0);
//             s += sizeof(__m256d);
//             d += sizeof(__m256d);
//             n -= sizeof(__m256d);
//         }
//     }
//     if (n)
//         memcpy(d, s, n);

//     // fencing is needed because of the use of non-temporal stores
//     _mm_sfence();

// #else
// #error "this file should be compiled with -mavx"
// #endif
//     return ret;
// }

// /** This function is to write data to the memory (in this case, part of the bridge).
//  * It uses AVX-512 instructions to write 64 bytes of data at a time.
//  * The function is designed to be run in multiple threads to benchmark the write speed.
//  * The first argument is the start address, and the second argument is the size of the data to be written (in bytes).
//  */
// void *write_bridge_sub_uc(void *args)
// {
//     size_t *arg = (size_t *)args;
//     memcpy_uncached_store_avx((void *)arg[0], (void *)arg[1], arg[2]);
//     return 0;
// }

// void *write_bridge_sub_wc(void *args)
// {
//     size_t *arg = (size_t *)args;
//     memcpy_cached_store_avx((void *)arg[0], (void *)arg[1], arg[2]);
//     return 0;
// }

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
    size_t size = 64 * 1024 * 1024; // size of data that a single thread should write (in bytes)
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
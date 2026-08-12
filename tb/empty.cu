#include <csignal>
#include <cstdio>

#include "cuda_runtime.h"

__global__ void empty(int* d_a, int* d_b, int* d_c) {
    // Keep every thread doing arithmetic work to sustain GPU utilization.
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int idx = tid % 100;
    int a = d_a[idx];
    int b = d_b[idx];

    unsigned int acc = static_cast<unsigned int>(tid + 1);
    for (int j = 0; j < 1000000; j++) {
        acc = acc * 1664525u + 1013904223u + static_cast<unsigned int>(a + b + idx + j);
    }

    d_c[tid] = static_cast<int>(acc);
}

static volatile sig_atomic_t g_keep_running = 1;

static void onSignal(int) { g_keep_running = 0; }

static void checkCuda(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        std::fprintf(stderr, "%s: %s\n", msg, cudaGetErrorString(err));
        std::exit(1);
    }
}

int main() {
    int *d_a, *d_b, *d_c;
    constexpr int kDataSize = 100;
    constexpr int kThreads = 1024;
    constexpr int kBlocks = 1;

    std::signal(SIGINT, onSignal);

    checkCuda(cudaMalloc(&d_a, kDataSize * sizeof(int)), "cudaMalloc d_a failed");
    checkCuda(cudaMalloc(&d_b, kDataSize * sizeof(int)), "cudaMalloc d_b failed");
    checkCuda(cudaMalloc(&d_c, kBlocks * kThreads * sizeof(int)), "cudaMalloc d_c failed");

    int h_a[kDataSize];
    int h_b[kDataSize];
    for (int i = 0; i < kDataSize; i++) {
        h_a[i] = i;
        h_b[i] = 100 - i;
    }
    checkCuda(cudaMemcpy(d_a, h_a, sizeof(h_a), cudaMemcpyHostToDevice), "Memcpy d_a failed");
    checkCuda(cudaMemcpy(d_b, h_b, sizeof(h_b), cudaMemcpyHostToDevice), "Memcpy d_b failed");

    unsigned long long launch_count = 0;
    while (g_keep_running) {
        empty<<<kBlocks, kThreads>>>(d_a, d_b, d_c);
        checkCuda(cudaGetLastError(), "Kernel launch failed");
        launch_count++;

        // Periodic sync to surface runtime errors while still keeping it busy.
        if ((launch_count % 1000ULL) == 0ULL) {
            checkCuda(cudaDeviceSynchronize(), "Kernel execution failed");
        }
    }

    checkCuda(cudaDeviceSynchronize(), "Final sync failed");
    std::printf("stopped, total launched kernels: %llu\n", launch_count);

    checkCuda(cudaFree(d_a), "cudaFree d_a failed");
    checkCuda(cudaFree(d_b), "cudaFree d_b failed");
    checkCuda(cudaFree(d_c), "cudaFree d_c failed");
    return 0;
}
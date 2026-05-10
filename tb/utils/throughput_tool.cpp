#include <fmt/args.h>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <unistd.h>

#include <QDMAController.hpp>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "dma_tool.hpp"

void cpu_throughput_h2c(uint8_t pci_bus) {
    fmt::println("=====CPU H2C throughput benchmark start=====");

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);
    auto cpu_mem_ctl = CPUMemCtl::getInstance(1UL * 1024 * 1024 * 1024);

    size_t pool_size = 1UL * 1024 * 1024 * 1024;

    cpu_mem_ctl->writeTLB([=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
        fpga_ctl->writeReg(8, (uint32_t)(vaddr));
        fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
        fpga_ctl->writeReg(10, (uint32_t)(paddr));
        fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
        fpga_ctl->writeReg(12, (page_index == 0));
        fpga_ctl->writeReg(13, 1);
        fpga_ctl->writeReg(13, 0);
    });

    auto dma_buff = cpu_mem_ctl->alloc(pool_size);
    auto p = (uint32_t*)dma_buff;

    // data length per cmd deliver
    uint32_t length = 1 * 1024;
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    uint32_t range = 1 * 1024 * 1024 * 1024;
    uint32_t range_words = range / 64;

    // initial dma buffer
    // FPGA: 512-bit = 64Byte = 16 * uint32_t
    for (int i = 0; i < range_words; i++) {
        for (int j = 0; j < 16; j++) {
            p[i * 16 + j] = i;
        }
    }

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(100, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(101, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(102, length);

    fpga_ctl->writeReg(104, total_cmds);
    fpga_ctl->writeReg(105, total_words);
    fpga_ctl->writeReg(106, range);
    fpga_ctl->writeReg(107, range_words);

    reset_counters(fpga_ctl);

    // start
    fpga_ctl->writeReg(103, 0);
    fpga_ctl->writeReg(103, 1);

    sleep(1);

    auto cycles = fpga_ctl->readReg(512 + 102);

    fmt::println("Number of errors: {}", fpga_ctl->readReg(512 + 101));
    fmt::println("Cycles: {}", cycles);

    double speed = 1.0 * length * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;

    fmt::println("Total length: {}", total_words * 64);
    fmt::println("Speed: {:.2f} GB/s", speed);

    fmt::print("Real words in q: {}", fpga_ctl->readReg(512 + 100));
    fmt::print("\n");

    cpu_mem_ctl->free(dma_buff);

    throughput_benchmark_print_counters(fpga_ctl);
}

void cpu_throughput_c2h(uint8_t pci_bus) {
    fmt::println("=====CPU C2H throughput benchmark start=====");

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);
    auto cpu_mem_ctl = CPUMemCtl::getInstance(1UL * 1024 * 1024 * 1024);

    size_t pool_size = 1L * 1024 * 1024 * 1024;

    cpu_mem_ctl->writeTLB([=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
        fpga_ctl->writeReg(8, (uint32_t)(vaddr));
        fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
        fpga_ctl->writeReg(10, (uint32_t)(paddr));
        fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
        fpga_ctl->writeReg(12, (page_index == 0));
        fpga_ctl->writeReg(13, 1);
        fpga_ctl->writeReg(13, 0);
    });

    auto dma_buff = cpu_mem_ctl->alloc(pool_size);
    volatile auto p = (uint32_t*)dma_buff;

    memset(p, 0, pool_size);

    uint32_t length = 1 * 1024;
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    printf("fpga_vaddr: 0x%lx\n", fpga_vaddr);
    fpga_ctl->writeReg(200, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(201, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(204, total_cmds);
    fpga_ctl->writeReg(205, total_words);

    reset_counters(fpga_ctl);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    printf("tag: %d\n", tag);
    fmt::println("{}", tag & 0x7f);
    fpga_ctl->writeReg(206, tag);

    fpga_ctl->writeReg(203, 0);  // start
    fpga_ctl->writeReg(203, 1);

    sleep(5);

    uint32_t count_cmd = fpga_ctl->readReg(512 + 200);
    uint32_t count_word = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);

    fmt::println("count cmd: {},right: {}", count_cmd, total_cmds);
    fmt::println("count word: {},right: {}", count_word, total_words);
    fmt::println("count time: {}", count_time);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed: {:.2f} GB/s", speed);

    uint32_t right_count = 0;
    uint32_t wrong_count = 0;

    // for (int i = 0; i < 4; i++) {
    //     printf("Packet [%03d]: ", i);
    //     for (int j = 0; j < 16; j++) {
    //         printf("%08x ", p[i * 16 + j]);
    //     }
    //     printf("\n");
    // }

    for (int i = 0; i < total_words; i++) {
        bool is_right = true;

        for (int j = 0; j < 16; j++) {
            if ((uint32_t)p[i * 16 + j] != i) {
                is_right = false;
                break;
            }
        }

        if (is_right) {
            right_count++;
        } else {
            wrong_count++;
        }
    }
    fmt::println("right data count: {}, wrong data count: {}\n", right_count, wrong_count);

    cpu_mem_ctl->free(dma_buff);

    throughput_benchmark_print_counters(fpga_ctl);
}

void gpu_throughput_h2c(uint8_t pci_bus) {
    fmt::println("=====GPU H2C throughput benchmark start=====");

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);
    auto gpu_mem_ctl = GPUMemCtl::getInstance(0, 1UL * 1024 * 1024 * 1024);

    size_t pool_size = 1UL * 1024 * 1024 * 1024;

    gpu_mem_ctl->writeTLB(
        [=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
            fpga_ctl->writeReg(8, (uint32_t)(vaddr));
            fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
            fpga_ctl->writeReg(10, (uint32_t)(paddr));
            fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
            fpga_ctl->writeReg(12, (page_index == 0));
            fpga_ctl->writeReg(13, 1);
            fpga_ctl->writeReg(13, 0);
        },
        true);

    // get buff (gpu vaddr)
    auto dma_buff = gpu_mem_ctl->alloc(pool_size);
    // gpu pool's base in gpu vaddr
    auto gpu_base = reinterpret_cast<uintptr_t>(gpu_mem_ctl->getDevPtr());
    auto host_base = static_cast<uint32_t*>(gpu_mem_ctl->getMapDevPtr());
    volatile auto p = host_base + (reinterpret_cast<uintptr_t>(dma_buff) - gpu_base) / sizeof(uint32_t);

    // data length per cmd deliver
    uint32_t length = 1 * 1024;
    uint32_t total_cmds = 1 * 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    uint32_t range = 1 * 1024 * 1024 * 1024;
    uint32_t range_words = range / 64;

    // initial dma buffer
    // FPGA: 512-bit = 64Byte = 16 * uint32_t
    for (int i = 0; i < range_words; i++) {
        for (int j = 0; j < 16; j++) {
            p[i * 16 + j] = i;
        }
    }

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(100, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(101, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(102, length);

    fpga_ctl->writeReg(104, total_cmds);
    fpga_ctl->writeReg(105, total_words);
    fpga_ctl->writeReg(106, range);
    fpga_ctl->writeReg(107, range_words);

    reset_counters(fpga_ctl);

    // start
    fpga_ctl->writeReg(103, 0);
    fpga_ctl->writeReg(103, 1);

    sleep(1);

    auto cycles = fpga_ctl->readReg(512 + 102);

    fmt::println("Number of errors: {}", fpga_ctl->readReg(512 + 101));
    fmt::println("Cycles: {}", cycles);

    double speed = 1.0 * length * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;

    fmt::println("Total length: {}", total_words * 64);
    fmt::println("Speed: {:.2f} GB/s", speed);

    fmt::print("Real words in q: {}", fpga_ctl->readReg(512 + 100));
    fmt::print("\n");

    gpu_mem_ctl->free(dma_buff);

    throughput_benchmark_print_counters(fpga_ctl);
    GPUMemCtl::cleanCtx();
}

void gpu_throughput_c2h(uint8_t pci_bus) {
    fmt::println("=====GPU C2H throughput benchmark start=====");

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);
    auto gpu_mem_ctl = GPUMemCtl::getInstance(0, 1UL * 1024 * 1024 * 1024);

    size_t pool_size = 1L * 1024 * 1024 * 1024;

    gpu_mem_ctl->writeTLB(
        [=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
            fpga_ctl->writeReg(8, (uint32_t)(vaddr));
            fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
            fpga_ctl->writeReg(10, (uint32_t)(paddr));
            fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
            fpga_ctl->writeReg(12, (page_index == 0));
            fpga_ctl->writeReg(13, 1);
            fpga_ctl->writeReg(13, 0);
        },
        true);

    auto dma_buff = gpu_mem_ctl->alloc(pool_size);
    auto gpu_base = reinterpret_cast<uintptr_t>(gpu_mem_ctl->getDevPtr());
    auto host_base = static_cast<uint32_t*>(gpu_mem_ctl->getMapDevPtr());
    volatile auto p = host_base + (reinterpret_cast<uintptr_t>(dma_buff) - gpu_base) / sizeof(uint32_t);

    memset(p, 0, pool_size);

    uint32_t length = 1 * 1024;
    uint32_t total_cmds = 1 * 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(200, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(201, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(204, total_cmds);
    fpga_ctl->writeReg(205, total_words);

    reset_counters(fpga_ctl);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    printf("tag: %d\n", tag);
    fmt::println("{}", tag & 0x7f);
    fpga_ctl->writeReg(206, tag);

    fpga_ctl->writeReg(203, 0);  // start
    fpga_ctl->writeReg(203, 1);

    sleep(3);

    uint32_t count_cmd = fpga_ctl->readReg(512 + 200);
    uint32_t count_word = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);

    fmt::println("count cmd: {},right: {}", count_cmd, total_cmds);
    fmt::println("count word: {},right: {}", count_word, total_words);
    fmt::println("count time: {}", count_time);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed: {:.2f} GB/s", speed);

    uint32_t right_count = 0;
    uint32_t wrong_count = 0;
    uint32_t value_verify = 0;

    for (int i = 0; i < 16384; i++, value_verify++) {
        uint32_t val = p[i * 16];
        if (value_verify != val) {
            wrong_count++;
            continue;
        }

        bool right = true;
        for (int j = 1; j < 16; j++) {
            if (p[i * 16 + j] != val) {
                wrong_count++;
                right = false;
                break;
            }
        }

        if (right) {
            right_count++;
        }
    }
    fmt::println("right data count: {}, wrong data count: {}\n", right_count, wrong_count);

    gpu_mem_ctl->free(dma_buff);
    fpga_ctl->writeReg(203, 0);

    throughput_benchmark_print_counters(fpga_ctl);
    GPUMemCtl::cleanCtx();
}
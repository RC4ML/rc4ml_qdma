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

void cpu_random_h2c(uint8_t pci_bus) {
    fmt::println("=====CPU H2C random benchmark start=====");

    size_t size = 1UL * 1024 * 1024 * 1024;

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);

    auto cpu_mem_ctl = CPUMemCtl::getInstance(1UL * 1024 * 1024 * 1024);

    cpu_mem_ctl->writeTLB([=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
        fpga_ctl->writeReg(8, (uint32_t)(vaddr));
        fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
        fpga_ctl->writeReg(10, (uint32_t)(paddr));
        fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
        fpga_ctl->writeReg(12, (page_index == 0));
        fpga_ctl->writeReg(13, 1);
        fpga_ctl->writeReg(13, 0);
    });

    auto dma_buff = cpu_mem_ctl->alloc(size);
    volatile auto p = (uint32_t*)dma_buff;

    uint32_t length = 1024;
    uint32_t busrt_length_shift = (uint32_t)log2(length);
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    for (int i = 0; i < size / 64; i++) {
        p[i * 16] = i * 64;
    }

    fpga_ctl->writeReg(100, (uint32_t)((unsigned long)p >> 32));
    fpga_ctl->writeReg(101, (uint32_t)((unsigned long)p));
    fpga_ctl->writeReg(102, length);
    fpga_ctl->writeReg(103, busrt_length_shift);
    fpga_ctl->writeReg(105, total_words);
    fpga_ctl->writeReg(106, total_cmds);

    reset_counters(fpga_ctl);

    // start
    fpga_ctl->writeReg(104, 0);
    fpga_ctl->writeReg(104, 1);

    sleep(1);

    uint32_t count_words = fpga_ctl->readReg(512 + 100);
    uint32_t count_cmds = fpga_ctl->readReg(512 + 102);
    uint32_t count_time = fpga_ctl->readReg(512 + 103);

    fmt::print("\n");
    fmt::println("count_total_words: 0x{}", count_words);
    fmt::println("count_err_data:    0x{}, should be 0x0", fpga_ctl->readReg(512 + 101));
    fmt::println("count_send_cmd:    0x{}", count_cmds);

    fmt::println("Cycles: {}", count_time);

    double ops = 1.0 * count_cmds * 1e9 / (1.0 * count_time * 4.0);
    fmt::println("OPS: {:.3f} MOPS", ops / 1e6);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;

    fmt::println("Total length: {}", total_words * 64);
    fmt::println("Speed: {:.2f} GB/s", speed);
    fmt::print("\n");

    cpu_mem_ctl->free(dma_buff);

    random_benchmark_print_counters(fpga_ctl);
}

void cpu_random_c2h(uint8_t pci_bus) {
    fmt::println("=====CPU C2H random benchmark start=====");
    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);

    auto cpu_mem_ctl = CPUMemCtl::getInstance(1UL * 1024 * 1024 * 1024);

    size_t size = 1L * 1024 * 1024 * 1024;

    cpu_mem_ctl->writeTLB([=](uint32_t page_index, uint32_t page_size, uint64_t vaddr, uint64_t paddr) {
        fpga_ctl->writeReg(8, (uint32_t)(vaddr));
        fpga_ctl->writeReg(9, (uint32_t)((vaddr) >> 32));
        fpga_ctl->writeReg(10, (uint32_t)(paddr));
        fpga_ctl->writeReg(11, (uint32_t)((paddr) >> 32));
        fpga_ctl->writeReg(12, (page_index == 0));
        fpga_ctl->writeReg(13, 1);
        fpga_ctl->writeReg(13, 0);
    });

    auto dma_buff = cpu_mem_ctl->alloc(size);
    volatile auto p = (uint32_t*)dma_buff;

    memset(p, 0, size);

    uint32_t length = 64;  // 32K has ever triggered the horrible bug
    uint32_t busrt_length_shift = (uint32_t)log2(length);
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    fpga_ctl->writeReg(200, (uint32_t)((unsigned long)p >> 32));
    fpga_ctl->writeReg(201, (uint32_t)((unsigned long)p));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(203, busrt_length_shift);
    fpga_ctl->writeReg(205, total_words);
    fpga_ctl->writeReg(206, total_cmds);

    reset_counters(fpga_ctl);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    fpga_ctl->writeReg(207, tag);
    fmt::println("{}", tag & 0x7f);

    fpga_ctl->writeReg(204, 0);  // start
    fpga_ctl->writeReg(204, 1);

    sleep(2);
    uint32_t count_cmds = fpga_ctl->readReg(512 + 200);
    uint32_t count_words = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);

    fmt::println("count cmd: {},right: {}", count_cmds, total_cmds);
    fmt::println("count word: {},right: {}", count_words, total_words);
    fmt::println("count time: {}", count_time);

    double ops = 1.0 * count_cmds * 1e9 / (1.0 * count_time * 4.0);
    fmt::println("OPS: {:.3f} MOPS", ops / 1e6);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed: {:.2f} GB/s", speed);

    int count_written_right_word = 0;
    for (int i = 0; i < size / 64; i++) {
        if (p[i * 16] == i * 64) {
            count_written_right_word++;
        }
    }
    fmt::println("count_written_right_word:{}, total words:{}", count_written_right_word, total_words);
    if (count_time > 2 * 603700) {
        random_benchmark_print_counters(fpga_ctl);
    }

    GPUMemCtl::cleanCtx();
}

void gpu_random_h2c(uint8_t pci_bus) {
    fmt::println("=====GPU H2C random benchmark start=====");

    size_t pool_size = 1UL * 1024 * 1024 * 1024;

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);

    auto gpu_mem_ctl = GPUMemCtl::getInstance(0, 1UL * 1024 * 1024 * 1024);

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

    uint32_t length = 1024;
    uint32_t busrt_length_shift = (uint32_t)log2(length);
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    for (int i = 0; i < pool_size / 64; i++) {
        p[i * 16] = i * 64;
    }

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(100, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(101, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(102, length);
    fpga_ctl->writeReg(103, busrt_length_shift);
    fpga_ctl->writeReg(105, total_words);
    fpga_ctl->writeReg(106, total_cmds);

    reset_counters(fpga_ctl);

    // start
    fpga_ctl->writeReg(104, 0);
    fpga_ctl->writeReg(104, 1);

    sleep(1);

    uint32_t count_words = fpga_ctl->readReg(512 + 100);
    uint32_t count_cmds = fpga_ctl->readReg(512 + 102);
    uint32_t count_time = fpga_ctl->readReg(512 + 103);

    fmt::print("\n");
    fmt::println("count_total_words: 0x{}", count_words);
    fmt::println("count_err_data:    0x{}, should be 0x0", fpga_ctl->readReg(512 + 101));
    fmt::println("count_send_cmd:    0x{}", count_cmds);

    fmt::println("Cycles: {}", count_time);

    double ops = 1.0 * count_cmds * 1e9 / (1.0 * count_time * 4.0);
    fmt::println("OPS: {:.3f} MOPS", ops / 1e6);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;

    fmt::println("Total length: {}", total_words * 64);
    fmt::println("Speed: {:.2f} GB/s", speed);
    fmt::print("\n");

    gpu_mem_ctl->free(dma_buff);

    random_benchmark_print_counters(fpga_ctl);
    GPUMemCtl::cleanCtx();
}

void gpu_random_c2h(uint8_t pci_bus) {
    fmt::println("=====GPU C2H random benchmark start=====");
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

    uint32_t length = 1024;  // 32K has ever triggered the horrible bug
    uint32_t busrt_length_shift = (uint32_t)log2(length);
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(200, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(201, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(203, busrt_length_shift);
    fpga_ctl->writeReg(205, total_words);
    fpga_ctl->writeReg(206, total_cmds);

    reset_counters(fpga_ctl);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    fpga_ctl->writeReg(207, tag);
    fmt::println("{}", tag & 0x7f);

    fpga_ctl->writeReg(204, 0);  // start
    fpga_ctl->writeReg(204, 1);

    sleep(2);
    uint32_t count_cmds = fpga_ctl->readReg(512 + 200);
    uint32_t count_words = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);

    fmt::println("count cmd: {},right: {}", count_cmds, total_cmds);
    fmt::println("count word: {},right: {}", count_words, total_words);
    fmt::println("count time: {}", count_time);

    double ops = 1.0 * count_cmds * 1e9 / (1.0 * count_time * 4.0);
    fmt::println("OPS: {:.3f} MOPS", ops / 1e6);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed: {:.2f} GB/s", speed);

    int count_written_right_word = 0;
    for (int i = 0; i < pool_size / 64; i++) {
        if (p[i * 16] == i * 64) {
            count_written_right_word++;
        }
    }
    fmt::println("count_written_right_word:{}, total words:{}", count_written_right_word, total_words);
    if (count_time > 2 * 603700) {
        random_benchmark_print_counters(fpga_ctl);
    }

    gpu_mem_ctl->free(dma_buff);
    GPUMemCtl::cleanCtx();
}
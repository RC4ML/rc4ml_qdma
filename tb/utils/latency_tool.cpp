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

void cpu_latency_h2c(uint8_t pci_bus) {
    fmt::println("=====H2C latency benchmark start=====");

    FPGACtl::explictInit(pci_bus, 4 * 1024 * 1024);
    auto fpga_ctl = FPGACtl::getInstance(pci_bus);

    auto cpu_mem_ctl = CPUMemCtl::getInstance(1UL * 1024 * 1024 * 1024);

    size_t size = 1UL * 1024 * 1024 * 1024;

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

    uint32_t length = 1 * 4 * 1024;
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;
    uint32_t wait_cycles = 50;  // 100=2.5Mops,when 4K burst, 100=10GB/s

    // initial dma buffer
    // FPGA: 512-bit = 64Byte = 16 * uint32_t
    for (int i = 0; i < total_words; i++) {
        for (int j = 0; j < 16; j++) {
            p[i * 16 + j] = i;
        }
    }

    fpga_ctl->writeReg(100, (uint32_t)((unsigned long)p >> 32));
    fpga_ctl->writeReg(101, (uint32_t)((unsigned long)p));
    fpga_ctl->writeReg(102, length);
    fpga_ctl->writeReg(104, total_cmds);
    fpga_ctl->writeReg(105, total_words);
    fpga_ctl->writeReg(106, wait_cycles);

    // start
    fpga_ctl->writeReg(103, 0);
    fpga_ctl->writeReg(103, 1);

    sleep(3);
    unsigned int cycles = fpga_ctl->readReg(512 + 103);
    fmt::print("\n");
    fmt::println("burst length: {}", length);

    fmt::println("count_total_words: 0x{:x}, shoule be 0x{:x}", fpga_ctl->readReg(512 + 100), total_words);
    fmt::println("count_send_cmd:    0x{:x}, shoule be 0x{:x}", fpga_ctl->readReg(512 + 101), total_cmds);
    fmt::println("count_err_data:    0x{:x}, should be 0x0", fpga_ctl->readReg(512 + 102));

    fmt::println("Cycles: {}", cycles);
    double speed = 1.0 * length * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Total length: {}", total_words * 64);
    fmt::println("Speed: {:.1f} GB/s", speed);

    size_t count_latency = (((size_t)fpga_ctl->readReg(512 + 105)) << 32) + fpga_ctl->readReg(512 + 104);
    fmt::println("count_latency        0x{:x}", count_latency);
    fmt::println("wait cycles          {}", wait_cycles);
    double average_latency = 1.0 * count_latency * 4 / total_cmds / 1000;  // us
    fmt::println("average_latency	   {:.2f} us", average_latency);
    double ops_limit = 1.0 * 250 * 1024 * 1024 / wait_cycles / 1024 / 1024;  // Mps
    fmt::println("ops_limit            {:.1f} Mops", ops_limit);
    double ops = 1.0 * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024;  // Mps
    fmt::println("ops                  {:.1f} Mops", ops);
    fmt::print("\n");
    // printCounters();
    cpu_mem_ctl->free(dma_buff);
}

void cpu_latency_c2h(uint8_t pci_bus) {
    fmt::println("=====C2H latency benchmark start=====");
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

    // the num of Bytes(promise) per command
    // uint32_t: define how to explain the data(how many bit per step)
    uint32_t length = 1 * 4 * 1024;
    uint32_t total_cmds = 1 * 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;
    uint32_t wait_cycles = 50;  // 100=2.5Mops,when 4K burst, 100=10GB/s

    fpga_ctl->writeReg(200, (uint32_t)((unsigned long)p >> 32));
    fpga_ctl->writeReg(201, (uint32_t)((unsigned long)p));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(204, total_words);
    fpga_ctl->writeReg(205, total_cmds);
    fpga_ctl->writeReg(207, wait_cycles);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    fpga_ctl->writeReg(206, tag);

    fmt::println("{}", tag & 0x7f);

    // length: Bytes per command
    // FPGA: 512bits = 64Bytes
    // beats: how many 512bits data in one command
    int beats = length / 64;
    volatile uint32_t* p_ack = p;

    fpga_ctl->writeReg(203, 0);  // start
    fpga_ctl->writeReg(203, 1);

    for (int i = 0; i < total_cmds; i++) {
        uint32_t verifiy_value = i * beats;

        while (true) {
            bool done = p_ack[i * beats * 16] == verifiy_value;
            // printf("p_ack[i * beats * 16]: %08x, verify_value: %08x\n", p_ack[i * beats * 16], verifiy_value);
            if (done) {
                break;
            }
        }
        fpga_ctl->writeBridge(0, {1, 1, 1, 1, 1, 1, 1, 1});
    }

    uint32_t count_cmds = fpga_ctl->readReg(512 + 200);
    uint32_t count_words = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);
    uint32_t count_recv_ack = fpga_ctl->readReg(512 + 207);

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

    fmt::println("burst length: {}", length);
    fmt::println("count_cmds:     0x{:x},should be: 0x{:x}", count_cmds, total_cmds);
    fmt::println("count_recv_ack: 0x{:x},should be: 0x{:x}", count_recv_ack, total_cmds);
    fmt::println("count_words:    0x{:x},should be: 0x{:x}", count_words, total_words);
    fmt::println("count_error:    0x{:x},shoule be: 0x0", wrong_count);
    fmt::println("count time: {}", count_time);

    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed: {:.2f} GB/s", speed);

    size_t count_latency_cmd = (((size_t)fpga_ctl->readReg(512 + 204)) << 32) + fpga_ctl->readReg(512 + 203);
    size_t count_latency_data = (((size_t)fpga_ctl->readReg(512 + 206)) << 32) + fpga_ctl->readReg(512 + 205);
    fmt::println("count_latency_cmd      0x{:x}", count_latency_cmd);
    fmt::println("count_latency_data     0x{:x}", count_latency_data);

    double average_latency_cmd = 1.0 * count_latency_cmd * 4 / total_cmds / 1000;               // us
    double average_latency_data = 1.0 * count_latency_data * 4 / total_cmds / 1000;             // us
    double ops_limit = 1.0 * 250 * 1024 * 1024 / wait_cycles / 1024 / 1024;                     // Mps
    double ops = 1.0 * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024;  // Mps
    fmt::println("wait cycles              {}", wait_cycles);
    fmt::println("average_latency_cmd      {:.1f} us", average_latency_cmd);
    fmt::println("average_latency_data     {:.1f} us", average_latency_data);
    fmt::println("ops_limit                {:.1f} Mops", ops_limit);
    fmt::println("ops                      {:.1f} Mops", ops);
    // printCounters();
    cpu_mem_ctl->free(dma_buff);
}

void gpu_latency_h2c(uint8_t pci_bus) {
    fmt::println("=====GPU H2C latency benchmark start=====");

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

    uint32_t length = 1 * 4 * 1024;
    uint32_t total_cmds = 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;
    uint32_t wait_cycles = 50;  // 100=2.5Mops,when 4K burst, 100=10GB/s

    // initial dma buffer
    // FPGA: 512-bit = 64Byte = 16 * uint32_t
    for (int i = 0; i < total_words; i++) {
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
    fpga_ctl->writeReg(106, wait_cycles);

    // start
    fpga_ctl->writeReg(103, 0);
    fpga_ctl->writeReg(103, 1);

    sleep(3);
    unsigned int cycles = fpga_ctl->readReg(512 + 103);
    fmt::print("\n");
    fmt::println("burst length: {}", length);

    fmt::println("count_total_words: 0x{:x}, shoule be 0x{:x}", fpga_ctl->readReg(512 + 100), total_words);
    fmt::println("count_send_cmd:    0x{:x}, shoule be 0x{:x}", fpga_ctl->readReg(512 + 101), total_cmds);
    fmt::println("count_err_data:    0x{:x}, should be 0x0", fpga_ctl->readReg(512 + 102));

    fmt::println("Cycles: {}", cycles);
    fmt::println("Total length: {}", total_words * 64);

    size_t count_latency = (((size_t)fpga_ctl->readReg(512 + 105)) << 32) + fpga_ctl->readReg(512 + 104);
    fmt::println("count_latency        0x{:x}", count_latency);
    fmt::println("wait cycles          {}", wait_cycles);

    double ops_limit = 1.0 * 250 * 1024 * 1024 / wait_cycles / 1024 / 1024;  // Mps
    fmt::println("ops_limit            {:.1f} Mops", ops_limit);
    double ops = 1.0 * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024;  // Mps
    fmt::println("ops                  {:.1f} Mops", ops);

    double speed = 1.0 * length * total_cmds / (1.0 * cycles * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    fmt::println("Speed:               {:.1f} GB/s", speed);

    double average_latency = 1.0 * count_latency * 4 / total_cmds / 1000;  // us
    fmt::println("average_latency	   {:.2f} us", average_latency);

    fmt::print("\n");
    // printCounters();
    gpu_mem_ctl->free(dma_buff);
    GPUMemCtl::cleanCtx();
}

void gpu_latency_c2h(uint8_t pci_bus) {
    fmt::println("=====GPU C2H latency benchmark start=====");
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

    // the num of Bytes(promise) per command
    // uint32_t: define how to explain the data(how many bit per step)
    uint32_t length = 1 * 4 * 1024;
    uint32_t total_cmds = 1 * 256 * 1024;
    uint32_t total_words = length / 64 * total_cmds;
    uint32_t wait_cycles = 100;  // 100=2.5Mops,when 4K burst, 100=10GB/s

    auto fpga_vaddr = reinterpret_cast<uint64_t>(dma_buff);
    fpga_ctl->writeReg(200, (uint32_t)(fpga_vaddr >> 32));
    fpga_ctl->writeReg(201, (uint32_t)(fpga_vaddr));
    fpga_ctl->writeReg(202, length);
    fpga_ctl->writeReg(204, total_words);
    fpga_ctl->writeReg(205, total_cmds);
    fpga_ctl->writeReg(207, wait_cycles);

    fpga_ctl->writeConfig(0x1408 / 4, 0);
    uint32_t tag = fpga_ctl->readConfig(0x140c / 4);
    fpga_ctl->writeReg(206, tag);

    fmt::println("{}", tag & 0x7f);

    // length: Bytes per command
    // FPGA: 512bits = 64Bytes
    // beats: how many 512bits data in one command
    int beats = length / 64;
    volatile uint32_t* p_ack = p;

    fpga_ctl->writeReg(203, 0);  // start
    fpga_ctl->writeReg(203, 1);

    for (int i = 0; i < total_cmds; i++) {
        uint32_t verifiy_value = i * beats;

        while (true) {
            bool done = p_ack[i * beats * 16] == verifiy_value;
            // printf("p_ack[i * beats * 16]: %08x, verify_value: %08x\n", p_ack[i * beats * 16], verifiy_value);
            if (done) {
                break;
            }
        }
        fpga_ctl->writeBridge(0, {1, 1, 1, 1, 1, 1, 1, 1});
    }

    uint32_t count_cmds = fpga_ctl->readReg(512 + 200);
    uint32_t count_words = fpga_ctl->readReg(512 + 201);
    uint32_t count_time = fpga_ctl->readReg(512 + 202);
    uint32_t count_recv_ack = fpga_ctl->readReg(512 + 207);

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

    // for (int i = 0; i < 4; i++) {
    //     printf("Packet [%03d]: ", i);
    //     for (int j = 0; j < 16; j++) {
    //         printf("%08x ", p[i * 16 + j]);
    //     }
    //     printf("\n");
    // }

    fmt::println("burst length: {}", length);
    fmt::println("count_cmds:     0x{:x},should be: 0x{:x}", count_cmds, total_cmds);
    fmt::println("count_recv_ack: 0x{:x},should be: 0x{:x}", count_recv_ack, total_cmds);
    fmt::println("count_words:    0x{:x},should be: 0x{:x}", count_words, total_words);
    fmt::println("count_error:    0x{:x},shoule be: 0x0", wrong_count);
    fmt::println("count time: {}", count_time);

    size_t count_latency_cmd = (((size_t)fpga_ctl->readReg(512 + 204)) << 32) + fpga_ctl->readReg(512 + 203);
    size_t count_latency_data = (((size_t)fpga_ctl->readReg(512 + 206)) << 32) + fpga_ctl->readReg(512 + 205);
    fmt::println("count_latency_cmd      0x{:x}", count_latency_cmd);
    fmt::println("count_latency_data     0x{:x}", count_latency_data);

    double ops_limit = 1.0 * 250 * 1024 * 1024 / wait_cycles / 1024 / 1024;                     // Mps
    double ops = 1.0 * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024;  // Mps
    double speed = 1.0 * length * total_cmds / (1.0 * count_time * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024;
    double average_latency_cmd = 1.0 * count_latency_cmd * 4 / total_cmds / 1000;    // us
    double average_latency_data = 1.0 * count_latency_data * 4 / total_cmds / 1000;  // us

    fmt::println("wait cycles              {}", wait_cycles);
    fmt::println("ops_limit                {:.1f} Mops", ops_limit);
    fmt::println("ops                      {:.1f} Mops", ops);
    fmt::println("Speed:                   {:.2f} GB/s", speed);
    fmt::println("average_latency_cmd      {:.1f} us", average_latency_cmd);
    fmt::println("average_latency_data     {:.1f} us", average_latency_data);
    // printCounters();
    gpu_mem_ctl->free(dma_buff);
    GPUMemCtl::cleanCtx();
}
#include "load.hpp"
#include <QDMAController.hpp>
#include <unistd.h>
#include <ctype.h>
#include <ctime>

void pauseFpgaH2C(FPGACtl *fpga_ctl) {
    fpga_ctl->writeReg(H2C_DATA_START, 0);
}

void resumeFpgaH2C(FPGACtl *fpga_ctl) {
    fpga_ctl->writeReg(H2C_DATA_START, 1);
}

bool startFpgaH2C(uint32_t *buffer, FPGACtl *fpga_ctl) {
    uint32_t addressHigh = (uint32_t)((unsigned long) (buffer) >> 32);
    uint32_t addressLow = (uint32_t)((unsigned long) (buffer));
    uint32_t dataLength = DEFAULT_DATA_BYTE_LENGTH;
    uint32_t sop = 1;
    uint32_t eop = 1;
    uint32_t totalCommands = DEFAULT_COMMANDS;
    uint32_t totalWords = dataLength / 64 * totalCommands;

    for (uint32_t i = 0; i < totalWords; i++) {
        for (uint32_t j = 0; j < 16; j++) {
            buffer[16 * i + j] = i;
        }
    }
    fpga_ctl->writeReg(H2C_ADDRESS_HIGH, addressHigh);
    fpga_ctl->writeReg(H2C_ADDRESS_LOW, addressLow);
    fpga_ctl->writeReg(H2C_DATA_BYTE_LENGTH, dataLength);
    fpga_ctl->writeReg(H2C_DATA_SOP, sop);
    fpga_ctl->writeReg(H2C_DATA_EOP, eop);
    fpga_ctl->writeReg(H2C_DATA_TOTAL_COMMANDS, totalCommands);
    fpga_ctl->writeReg(H2C_DATA_TOTAL_WORDS, totalWords);

    sleep(1);
    fmt::print(fg(fmt::color::cyan), "H2C Verification Start!\n");
    auto timeCountBegin = fpga_ctl->readReg(H2C_TIME_COUNT);
    resumeFpgaH2C(fpga_ctl);    // start
    sleep(2);
    pauseFpgaH2C(fpga_ctl);
    sleep(2);               // end
    auto errorCount = fpga_ctl->readReg(H2C_ERROR_COUNT);
    auto timeCountEnd = fpga_ctl->readReg(H2C_TIME_COUNT);
    auto timeCount = timeCountEnd - timeCountBegin;
    fmt::print(fg(fmt::color::cyan), "H2C Verification Complete!\n");
    fmt::println("Number of Errors: {}, Number of Cycles: {}", errorCount, timeCount);

    // verification
    auto commandsGenerated = fpga_ctl->readReg(H2C_COMMANDS_STATISTICS);
    auto commandsEmitted = fpga_ctl->readReg(H2C_COMMANDS_CHECK);
    fmt::println("io.h2c_cmd: {} fifo_h2c_cmd.io.out: {}", commandsGenerated, commandsEmitted);
    if (commandsGenerated == commandsGenerated && errorCount == 0) {
        fmt::println("H2C Check Passed! Bandwidth: {:.2f}GBps",
            1.0 * dataLength * commandsEmitted / (1.0 * timeCount * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024
        );
        return true;
    } else {
        fmt::print(fg(fmt::color::red), "H2C Check Failed! Benchmark Aborted!\n");
        return false;
    }

}


void pauseFpgaC2H(FPGACtl *fpga_ctl) {
    fpga_ctl->writeReg(C2H_DATA_START, 0);
}

void resumeFpgaC2H(FPGACtl *fpga_ctl) {
    fpga_ctl->writeReg(C2H_DATA_START, 1);
}

bool startFpgaC2H(volatile uint32_t *buffer, FPGACtl *fpga_ctl) {
    uint32_t addressHigh = (uint32_t)((unsigned long) (buffer) >> 32);
    uint32_t addressLow = (uint32_t)((unsigned long) (buffer));
    uint32_t dataLength = DEFAULT_DATA_BYTE_LENGTH;
    uint32_t totalCommands = DEFAULT_COMMANDS;
    uint32_t totalWords = dataLength / 64 * totalCommands;

    for (uint32_t i = 0; i < totalWords; i++) {
        for (uint32_t j = 0; j < 16; j++) {
            buffer[16 * i + j] = 0;
        }
    }

    fpga_ctl->writeReg(C2H_ADDRESS_HIGH, addressHigh);
    fpga_ctl->writeReg(C2H_ADDRESS_LOW, addressLow);
    fpga_ctl->writeReg(C2H_DATA_BYTE_LENGTH, dataLength);
    fpga_ctl->writeReg(C2H_DATA_TOTAL_COMMANDS, totalCommands);
    fpga_ctl->writeReg(C2H_DATA_TOTAL_WORDS, totalWords);
    fpga_ctl->writeConfig(0x1408 / 4, 0);
    auto tag = fpga_ctl->readConfig(0x140c / 4);
    fpga_ctl->writeReg(C2H_PFCH_TAG, tag);
    fpga_ctl->writeReg(C2H_TAG_INDEX, 0);

    sleep(1);
    fmt::print(fg(fmt::color::cyan), "C2H Verification Start!\n");
    auto timeCountBegin = fpga_ctl->readReg(C2H_TIME_COUNT);
    resumeFpgaC2H(fpga_ctl);       // start
    sleep(2);
    pauseFpgaC2H(fpga_ctl);        // end
    sleep(2);
    auto timeCountEnd = fpga_ctl->readReg(C2H_TIME_COUNT);
    auto wordsCount = fpga_ctl->readReg(C2H_WORDS_COUNT);
    auto commandsCount = fpga_ctl->readReg(C2H_COMMANDS_COUNT);
    auto timeCount = timeCountEnd - timeCountBegin;
    fmt::print(fg(fmt::color::cyan), "C2H Verification Complete!\n");

    uint32_t errorCount = 0;
    for (uint32_t i = 0; i < totalWords; i++) {
        if (buffer[16 * i] != i) {
            errorCount++;
        }
    }
    fmt::println("Number of Errors: {}, Number of Cycles: {}, Number of Words: {}, Number of Commands: {}",
        errorCount, timeCount, wordsCount, commandsCount
    );

    // verification
    auto commandsGenerated = fpga_ctl->readReg(C2H_COMMANDS_STATISTICS);
    auto commandsEmitted = fpga_ctl->readReg(C2H_COMMANDS_CHECK);
    if (commandsGenerated == commandsGenerated && errorCount == 0) {
        fmt::println("C2H Check Passed! Bandwidth: {:.2f} GBps",
            1.0 * dataLength * commandsEmitted / (1.0 * timeCount * 4 / 1000 / 1000 / 1000) / 1024 / 1024 / 1024
        );
        return true;
    } else {
        fmt::print(fg(fmt::color::red), "C2H Check Failed! Benchmark Aborted!\n");
        return false;
    }

}

void axilBenchmarkInit(FPGACtl *fpga_ctl) {
    for (int i = 0; i < 256; i++) {
        fpga_ctl->writeReg(256 + i, 512 + 256 + i);
        fpga_ctl->writeReg(512 + 256 + i, 256 + i + 1);
    }
}

uint64_t axilReadBenchmark(FPGACtl *fpga_ctl) {
    uint32_t index;
#ifdef ENABLE_CTIME
    struct timespec start_timer, end_timer;
    clock_gettime(CLOCK_MONOTONIC, &start_timer);
#else
    auto start = chrono::system_clock::now();
#endif
    index = fpga_ctl->readReg(256);
    for (int i = 0; i < 511; i++) {
        index = fpga_ctl->readReg(index);
    }
#ifdef ENABLE_CTIME
    clock_gettime(CLOCK_MONOTONIC, &end_timer);
    return ((end_timer.tv_sec - start_timer.tv_sec) * 1e9 + end_timer.tv_nsec - start_timer.tv_nsec);
#else
    auto end = chrono::system_clock::now();
    auto duration_us = chrono::duration_cast<chrono::nanoseconds>(end - start);
    return duration_us.count();
#endif

}

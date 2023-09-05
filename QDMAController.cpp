#include "QDMAController.h"
#include "QDMAController.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <fmt/os.h>
#include <fmt/args.h>
#include <fmt/ostream.h>
#include <fmt/std.h>	
#include <fmt/color.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <immintrin.h>

#include <rc4ml.h>

static bool debug_flag = false;

static auto getSysPathBarName(uint8_t bus_id, uint8_t dev_id, uint8_t func_id, uint8_t bar_id) {
	return fmt::format("/sys/bus/pci/devices/0000:{:02x}:{:02x}.{:x}/resource{}", bus_id, dev_id, func_id, bar_id);
}

static void errorPrint(std::string_view str){
	fmt::print(fg(fmt::color::red), "{}\n", str);
}

static void passPrint(std::string_view str){
	fmt::print(fg(fmt::color::green), "{}\n", str);
}

static void warnPrint(std::string_view str){
	fmt::print(fg(fmt::color::yellow), "{}\n", str);
}

static void infoPrint(std::string_view str){
	fmt::print(fg(fmt::color::cyan), "{}\n", str);
}

static const size_t config_region_size = 256*1024;
static const size_t lite_region_size = 4*1024;
static const size_t bridge_region_size = 1024*1024*1024;

static std::map<uint8_t, std::shared_ptr<FPGACtl>> device_list;

FPGACtl::FPGACtl(uint8_t pci_bus,
                 size_t bridge_bar_size):pci_bus(pci_bus),
                               bridge_bar_size(bridge_bar_size) {
    passPrint(fmt::format(
            "Init pci dev: 0x{:#x}",pci_bus));

    std::string resourceFilename;
    int fd;
    uint8_t pci_dev  =	0;
    uint8_t dev_func =	0;

    //axi-lite
    resourceFilename = getSysPathBarName(pci_bus, pci_dev, dev_func, 2); //lite bar is 2

    fd = open(resourceFilename.c_str(), O_RDWR);
    if (fd < 0){
        errorPrint(fmt::format(
                "Open lite error, maybe need sudo or you can check whether if {} exists", resourceFilename));
        exit(1);
    }
    lite_bar =(uint32_t*) mmap(nullptr, lite_region_size, PROT_WRITE, MAP_SHARED, fd, 0);
    // safe to close fd after mmap
    close(fd);
    if(lite_bar == MAP_FAILED) {
        errorPrint(fmt::format(
                "MMAP lite bar error, please check fpga lite bar size in vivado"));
        exit(1);
    }

    //axi-bridge
    resourceFilename = getSysPathBarName(pci_bus, pci_dev, dev_func, 4); //bridge bar is 4

    fd = open(resourceFilename.c_str(), O_RDWR);
    if (fd < 0) {
        errorPrint(fmt::format(
                "Open bridge error, maybe need sudo or you can check whether if {} exists", resourceFilename));
        exit(1);
    }
    bridge_bar =(__m512i*) mmap(nullptr, bridge_bar_size, PROT_WRITE, MAP_SHARED|MAP_LOCKED , fd, 0);
    close(fd);
    if(bridge_bar == MAP_FAILED) {
        errorPrint(fmt::format(
                "MMAP bridge bar error, please check fpga bridge bar size in vivado"));
        exit(1);
    }

    //config bar
    resourceFilename = getSysPathBarName(pci_bus, pci_dev, dev_func, 0); //config bar is 0

    fd = open(resourceFilename.c_str(), O_RDWR);
    if (fd < 0){
        errorPrint(fmt::format(
                "Open config error, maybe need sudo or you can check whether if {} exists", resourceFilename));
        exit(1);
    }
    config_bar = (uint32_t *)mmap(nullptr, config_region_size, PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if(config_bar == MAP_FAILED){
        errorPrint(fmt::format(
                "MMAP config bar error, please check fpga config bar size in vivado"));
        exit(1);
    }
}

FPGACtl::~FPGACtl() {
    if(munmap((void *) config_bar, config_region_size)==-1) {
        warnPrint(fmt::format("Unmap config bar failed"));
    }
    if(munmap((void *) lite_bar, lite_region_size)==-1) {
        warnPrint(fmt::format("Unmap lite bar failed"));
    }
    if(munmap((void *) bridge_bar, bridge_bar_size)==-1) {
        warnPrint(fmt::format("Unmap bridge bar failed"));
    }
}

void FPGACtl::explictInit(uint8_t pci_bus, size_t bridge_bar_size) {
    if(device_list.find(pci_bus)==device_list.end()) {
        auto *tmp = new FPGACtl(pci_bus, bridge_bar_size);
        device_list[pci_bus] = std::shared_ptr<FPGACtl>(tmp);
    } else {
        warnPrint(fmt::format("Device {:#x} has been initialized", pci_bus));
    }
}

FPGACtl *FPGACtl::getInstance(uint8_t pci_bus) {
    if(device_list.find(pci_bus) == device_list.end()) {
        warnPrint(fmt::format("Device {:#x} will be initialized with default Bridge Bar Size {}", pci_bus, bridge_region_size));
        auto *tmp = new FPGACtl(pci_bus, bridge_region_size);
        device_list[pci_bus] = std::shared_ptr<FPGACtl>(tmp);
    }
    return device_list[pci_bus].get();
}

void FPGACtl::writeConfig(uint32_t index, uint32_t value) {
    config_bar[index] = value;
}

uint32_t FPGACtl::readConfig(uint32_t index) {
    return config_bar[index];
}

void FPGACtl::writeReg(uint32_t index, uint32_t value) {
    lite_bar[index] = value;
}

uint32_t FPGACtl::readReg(uint32_t index) {
    return lite_bar[index];
}

void FPGACtl::writeBridge(uint32_t index, const std::array<uint64_t, 8> &value) {
    auto avx_reg = _mm512_loadu_epi64(value.data());
    _mm512_stream_si512((__m512i *)(bridge_bar + index), avx_reg);
}

std::array<uint64_t, 8> FPGACtl::readBridge(uint32_t index) {
    auto avx_reg = _mm512_stream_load_si512((void*)(bridge_bar + index));
    alignas(64) std::array<uint64_t, 8> ret;
    _mm512_store_epi64(ret.data(), avx_reg);
    return ret;
}

void FPGACtl::writeBridge(uint32_t index, uint64_t *value) {
    auto avx_reg = _mm512_loadu_epi64(value);
    _mm512_stream_si512((__m512i *)(bridge_bar + index), avx_reg);
}

void FPGACtl::readBridge(uint32_t index, uint64_t *value) {
    auto avx_reg = _mm512_stream_load_si512((void*)(bridge_bar + index));
    _mm512_storeu_epi64(value, avx_reg);
}

void FPGACtl::writeBridgeAligned(uint32_t index, uint64_t *value) {
    auto avx_reg = _mm512_load_epi64(value);
    _mm512_stream_si512((__m512i *)(bridge_bar + index), avx_reg);
}

void FPGACtl::readBridgeAligned(uint32_t index, uint64_t *value) {
    auto avx_reg = _mm512_stream_load_si512((void*)(bridge_bar + index));
    _mm512_store_epi64(value, avx_reg);
}

void *FPGACtl::getBridgeAddr() {
    return (void*)bridge_bar;
}

void *FPGACtl::getLiteAddr() {
    return (void*)lite_bar;
}

void FPGACtl::enableDebug() {
    debug_flag = true;
}

void FPGACtl::disableDebug() {
    debug_flag = false;
}

extern "C" {

void init(uint8_t pci_bus, size_t bridge_bar_size){
    FPGACtl::explictInit(pci_bus, bridge_bar_size);
}

void* qdmaCPUAlloc(size_t size, uint8_t pci_bus){
    return nullptr;
//    pci_bus = get_pci_bus(pci_bus);
//    int fd,hfd;
//    void* huge_base;
//    struct huge_mem hm;
//    std::string dev_path = fmt::format("/dev/rc4ml_dev");
//    if ((fd = open(dev_path.c_str() ,O_RDWR)) == -1) {
//        errorPrint(fmt::format("Open {rc4ml_dev} error, maybe need sudo or you can check whether if {rc4ml_dev} exists", fmt::arg("rc4ml_dev", dev_path)));
//        exit(1);
//    }
//
//    std::string hfd_path = fmt::format("/media/huge/hfd_{:x}",pci_bus);
//    if ((hfd = open(hfd_path.c_str(), O_CREAT | O_RDWR | O_SYNC, 0755)) == -1) {
//        errorPrint(fmt::format("Open {fn} error, maybe need sudo or you can check whether if {fn} exists", fmt::arg("fn", hfd_path)));
//        exit(1);
//    }
//
//    huge_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, hfd, 0);
//    passPrint(fmt::format("huge pages base vaddr:{}", fmt::ptr(huge_base)));
//
//    hm.vaddr = (unsigned long)huge_base;
//    hm.size = size;
//    if(ioctl(fd, HUGE_MAPPING_SET, &hm) == -1){
//        errorPrint(fmt::format("IOCTL SET failed."));
//        exit(1);
//    }
//    struct huge_mapping map;
//    map.nhpages = size/(2*1024*1024);
//    map.phy_addr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
//    if (ioctl(fd, HUGE_MAPPING_GET, &map) == -1) {
//        errorPrint(fmt::format("IOCTL GET failed."));
//        exit(1);
//    }
//
//    tlb* page_table = (tlb*)calloc(1,sizeof(tlb));
//    page_table->npages = map.nhpages;
//    page_table->vaddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
//    page_table->paddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
//    for(int i=0;i<page_table->npages;i++){
//        page_table->vaddr[i] = (unsigned long)huge_base + ((unsigned long)i)*2*1024*1024;
//        page_table->paddr[i] = map.phy_addr[i];
//    }
//
//    for(int i=0;i<page_table->npages;i++){
//        if(debug_flag){
//            fmt::println("VAddr: {:#016x} PAddr: {:#016x}", page_table->vaddr[i], page_table->paddr[i]);
//        }
//        device_list[pci_bus].lite_bar[8]	= (uint32_t)(page_table->vaddr[i]);
//        device_list[pci_bus].lite_bar[9]	= (uint32_t)((page_table->vaddr[i])>>32);
//        device_list[pci_bus].lite_bar[10]	= (uint32_t)(page_table->paddr[i]);
//        device_list[pci_bus].lite_bar[11]	= (uint32_t)((page_table->paddr[i])>>32);
//        device_list[pci_bus].lite_bar[12]	= (i==0);
//        device_list[pci_bus].lite_bar[13]	= 1;
//        device_list[pci_bus].lite_bar[13]	= 0;
//    }
//    return huge_base;
}

void writeConfig(uint32_t index,uint32_t value, uint8_t pci_bus){
    device_list[pci_bus]->writeConfig(index, value);
}
uint32_t readConfig(uint32_t index, uint8_t pci_bus){
    return device_list[pci_bus]->readConfig(index);
}

void writeReg(uint32_t index,uint32_t value, uint8_t pci_bus){
    device_list[pci_bus]->writeReg(index, value);
}
uint32_t readReg(uint32_t index, uint8_t pci_bus){
    return device_list[pci_bus]->readReg(index);
}

void writeBridge(uint32_t index, uint64_t* value, uint8_t pci_bus){
    device_list[pci_bus]->writeBridge(index, value);
}

void readBridge(uint32_t index, uint64_t* value, uint8_t pci_bus){
    device_list[pci_bus]->readBridge(index, value);
}


void* getBridgeAddr(uint8_t pci_bus){
    return device_list[pci_bus]->getBridgeAddr();
}

void* getLiteAddr(uint8_t pci_bus){
    return device_list[pci_bus]->getLiteAddr();
}

void enableDebug() {
    FPGACtl::enableDebug();
}
void disableDebug(){
    FPGACtl::disableDebug();
}

} // extern "C"
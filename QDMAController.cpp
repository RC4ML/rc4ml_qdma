#include "QDMAController.h"

#include <cstdint>

#include <map>
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

struct Bars{
	volatile uint32_t *config_bar;
	volatile uint32_t *lite_bar;
	volatile __m512i *bridge_bar;
};

typedef struct{
	int npages;
	unsigned long* vaddr;
	unsigned long* paddr;
}tlb;

std::map<char,Bars> device_list;
int num_device=0;
unsigned char default_pci_bus=0;

auto getSysPathBarName(uint8_t bus_id, uint8_t dev_id, uint8_t func_id, uint8_t bar_id) {
	return fmt::format("/sys/bus/pci/devices/0000:{:02x}:{:02x}.{:x}/resource{}", bus_id, dev_id, func_id, bar_id);
}

void errorPrint(std::string_view str){
	fmt::print(fg(fmt::color::red), "{}\n", str);
}

void passPrint(std::string_view str){
	fmt::print(fg(fmt::color::green), "{}\n", str);
}

void warnPrint(std::string_view str){
	fmt::print(fg(fmt::color::yellow), "{}\n", str);
}

unsigned char get_pci_bus(unsigned char pci_bus){
	if(pci_bus==0){
		pci_bus=default_pci_bus;
	}
	if(device_list.count(pci_bus)==0){
		errorPrint(fmt::format("Device {:#x} has not been initialized", pci_bus));
		exit(1);
	}
	return pci_bus;
}

void init(unsigned char pci_bus, size_t bridge_bar_size){
	passPrint(fmt::format("Init pci dev: 0x{:#x}",pci_bus));
	if(device_list.count(pci_bus) != 0){
		errorPrint(fmt::format("device {:#x} has already been initialized!", pci_bus));
		exit(1);
	}else{
		if(device_list.size() == 0){
			default_pci_bus = pci_bus;
		}
		Bars t;
		device_list[pci_bus] = t;
	}
	std::string fname;
	int fd;
	unsigned char pci_dev 	=	0;
	unsigned char dev_func	=	0;

	//axi-lite
	fname = getSysPathBarName(pci_bus, pci_dev, dev_func, 2); //lite bar is 2

	fd = open(fname.c_str(), O_RDWR);
	if (fd < 0){
		errorPrint(fmt::format("Open lite error, maybe need sudo or you can check whether if {} exists",fname));
		exit(1);
	}
	device_list[pci_bus].lite_bar =(uint32_t*) mmap(NULL, 4*1024, PROT_WRITE, MAP_SHARED, fd, 0);
	if(device_list[pci_bus].lite_bar == MAP_FAILED) {
		errorPrint(fmt::format("MMAP lite bar error, please check fpga lite bar size in vivado"));
		exit(1);
	}
	//axi-bridge
	fname = getSysPathBarName(pci_bus, pci_dev, dev_func, 4); //bridge bar is 4

	fd = open(fname.c_str(), O_RDWR);
	if (fd < 0) {
		errorPrint(fmt::format("Open bridge error, maybe need sudo or you can check whether if {} exists",fname));
		exit(1);
	}
	device_list[pci_bus].bridge_bar =(__m512i*) mmap(NULL, bridge_bar_size, PROT_WRITE, MAP_SHARED|MAP_LOCKED , fd, 0);
	if(device_list[pci_bus].bridge_bar == MAP_FAILED) {
		errorPrint(fmt::format("MMAP bridge bar error, please check fpga bridge bar size in vivado"));
		exit(1);
	}
	//config bar
	fname = getSysPathBarName(pci_bus, pci_dev, dev_func, 0); //config bar is 0
	fd = open(fname.c_str(), O_RDWR);
	if (fd < 0){
		errorPrint(fmt::format("Open config error, maybe need sudo or you can check whether if {} exists",fname));
		exit(1);
	}
	device_list[pci_bus].config_bar = (uint32_t *)mmap(NULL, 256*1024, PROT_WRITE, MAP_SHARED, fd, 0);
	if(device_list[pci_bus].config_bar == MAP_FAILED){
		errorPrint(fmt::format("MMAP config bar error, please check fpga config bar size in vivado"));
		exit(1);
	}
}

void* qdma_alloc(size_t size, unsigned char pci_bus, bool print_addr){
	pci_bus = get_pci_bus(pci_bus);
	int fd,hfd;
	void* huge_base;
	struct huge_mem hm;
	std::string dev_path = fmt::format("/dev/rc4ml_dev");
	if ((fd = open(dev_path.c_str() ,O_RDWR)) == -1) {
		errorPrint(fmt::format("Open {rc4ml_dev} error, maybe need sudo or you can check whether if {rc4ml_dev} exists", fmt::arg("rc4ml_dev", dev_path)));
		exit(1);
   	}

	std::string hfd_path = fmt::format("/media/huge/hfd_{:x}",pci_bus);
	if ((hfd = open(hfd_path.c_str(), O_CREAT | O_RDWR | O_SYNC, 0755)) == -1) {
		errorPrint(fmt::format("Open {fn} error, maybe need sudo or you can check whether if {fn} exists", fmt::arg("fn", hfd_path)));
		exit(1);
   	}

	huge_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, hfd, 0);
	passPrint(fmt::format("huge pages base vaddr:{}", fmt::ptr(huge_base)));

	hm.vaddr = (unsigned long)huge_base;
	hm.size = size;
	if(ioctl(fd, HUGE_MAPPING_SET, &hm) == -1){
		errorPrint(fmt::format("IOCTL SET failed."));
		exit(1);
	}
	struct huge_mapping map;
	map.nhpages = size/(2*1024*1024);
	map.phy_addr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	if (ioctl(fd, HUGE_MAPPING_GET, &map) == -1) {
		errorPrint(fmt::format("IOCTL GET failed."));
		exit(1);
   	}

	tlb* page_table = (tlb*)calloc(1,sizeof(tlb));
	page_table->npages = map.nhpages;
	page_table->vaddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	page_table->paddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	for(int i=0;i<page_table->npages;i++){
		page_table->vaddr[i] = (unsigned long)huge_base + ((unsigned long)i)*2*1024*1024;
		page_table->paddr[i] = map.phy_addr[i];
	}

	for(int i=0;i<page_table->npages;i++){
		if(print_addr){
			fmt::println("VAddr: {:#016x} PAddr: {:#016x}", page_table->vaddr[i], page_table->paddr[i]);
		}
		device_list[pci_bus].lite_bar[8]	= (uint32_t)(page_table->vaddr[i]);
		device_list[pci_bus].lite_bar[9]	= (uint32_t)((page_table->vaddr[i])>>32);
		device_list[pci_bus].lite_bar[10]	= (uint32_t)(page_table->paddr[i]);
		device_list[pci_bus].lite_bar[11]	= (uint32_t)((page_table->paddr[i])>>32);
		device_list[pci_bus].lite_bar[12]	= (i==0);
		device_list[pci_bus].lite_bar[13]	= 1;
		device_list[pci_bus].lite_bar[13]	= 0;
	}
	return huge_base;
}

void writeConfig(uint32_t index,uint32_t value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].config_bar[index] = value;
}
uint32_t readConfig(uint32_t index, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return device_list[pci_bus].config_bar[index];
}

void writeReg(uint32_t index,uint32_t value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].lite_bar[index] = value;
}
uint32_t readReg(uint32_t index, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return device_list[pci_bus].lite_bar[index];
}

void writeBridge(uint32_t index, uint64_t* value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].bridge_bar[index] = _mm512_set_epi64(value[7],value[6],value[5],value[4],value[3],value[2],value[1],value[0]);
}

void readBridge(uint32_t index, uint64_t* value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	_mm512_store_epi64(value,device_list[pci_bus].bridge_bar[index]);
}


void* getBridgeAddr(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return (void*)(device_list[pci_bus].bridge_bar);
}

void* getLiteAddr(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return (void*)(device_list[pci_bus].lite_bar);
}

void resetCounters(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].lite_bar[14] = 1;
	device_list[pci_bus].lite_bar[14] = 0;
}

void printCounters(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	volatile uint32_t *axi_lite = device_list[pci_bus].lite_bar;
	warnPrint("\nQDMA debug info:\n");

	warnPrint(fmt::format("bar 1: {:#x} 	tlb miss count", axi_lite[512+1]));

	warnPrint("\nC2H CMD fire()");

	warnPrint(fmt::format("bar 2: {:#x} 	io.c2h_cmd", axi_lite[512+2]));
	warnPrint(fmt::format("bar 3: {:#x} 	check_c2h.io.out", axi_lite[512+3]));
	warnPrint(fmt::format("bar 4: {:#x} 	tlb.io.c2h_out", axi_lite[512+4]));
	warnPrint(fmt::format("bar 5: {:#x} 	boundary_split.io.cmd_out", axi_lite[512+5]));
	warnPrint(fmt::format("bar 6: {:#x} 	fifo_c2h_cmd.io.out", axi_lite[512+6]));

	warnPrint("\nH2C CMD fire()");
	warnPrint(fmt::format("bar 7: {:#x} 	io.h2c_cmd", axi_lite[512+7]));
	warnPrint(fmt::format("bar 8: {:#x} 	check_h2c.io.out", axi_lite[512+8]));
	warnPrint(fmt::format("bar 9: {:#x} 	tlb.io.h2c_out", axi_lite[512+9]));
	warnPrint(fmt::format("bar 10: {:#x} 	boundary_split.io.cmd_out", axi_lite[512+10]));

	warnPrint("\nC2H DATA fire()");
	warnPrint(fmt::format("bar 11: {:#x} 	io.c2h_data", axi_lite[512+11]));
	warnPrint(fmt::format("bar 12: {:#x} 	boundary_split.io.data_out", axi_lite[512+12]));
	warnPrint(fmt::format("bar 13: {:#x} 	fifo_c2h_data.io.out", axi_lite[512+13]));

	warnPrint("\nH2C DATA fire()");
	warnPrint(fmt::format("bar 14: {:#x} 	io.h2c_data", axi_lite[512+14]));
	warnPrint(fmt::format("bar 15: {:#x} 	fifo_h2c_data.io.in", axi_lite[512+15]));

	//reporter
	warnPrint("\nReporter");
	
	warnPrint(fmt::format("{} Report 0:boundary check state===sIDLE", ((axi_lite[512+16]>>0) & 1)));
	warnPrint(fmt::format("{} Report 1:boundary check state===sIDLE", ((axi_lite[512+16]>>1) & 1)));
	warnPrint(fmt::format("{} Report 2:boundary split state===sIDLE", ((axi_lite[512+16]>>2) & 1)));

	fmt::print("\n");

	warnPrint(fmt::format("{} Report 3:fifo_c2h_cmd.io.out.valid", ((axi_lite[512+16]>>3) & 1)));
	warnPrint(fmt::format("{} Report 4:fifo_c2h_cmd.io.out.ready", ((axi_lite[512+16]>>4) & 1)));

	fmt::print("\n");

	warnPrint(fmt::format("{} Report 5:fifo_h2c_cmd.io.out.valid", ((axi_lite[512+16]>>5) & 1)));
	warnPrint(fmt::format("{} Report 6:fifo_h2c_cmd.io.out.ready", ((axi_lite[512+16]>>6) & 1)));

	fmt::print("\n");

	warnPrint(fmt::format("{} Report 7:fifo_c2h_data.io.out.valid", ((axi_lite[512+16]>>7) & 1)));
	warnPrint(fmt::format("{} Report 8:fifo_c2h_data.io.out.ready", ((axi_lite[512+16]>>8) & 1)));

	fmt::print("\n");

	warnPrint(fmt::format("{} Report 9:fifo_h2c_data.io.in.valid", ((axi_lite[512+16]>>9) & 1)));
	warnPrint(fmt::format("{} Report 10:fifo_h2c_data.io.in.ready", ((axi_lite[512+16]>>10) & 1)));

	fmt::print("\n");
}
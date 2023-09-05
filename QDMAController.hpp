#ifndef _QDMACONTROLLER_HPP_
#define _QDMACONTROLLER_HPP_

#include <array>

#include <cstdint>
#include <immintrin.h>

class FPGACtl{
protected:
	FPGACtl(uint8_t pci_bus, size_t bridge_bar_size);
public:
	~FPGACtl();

	static void explictInit(uint8_t pci_bus, size_t bridge_bar_size);
	static FPGACtl* getInstance(uint8_t pci_bus);

    static void enableDebug();
    static void disableDebug();

public:
	void writeConfig(uint32_t index,uint32_t value);
	uint32_t readConfig(uint32_t index);

	void writeReg(uint32_t index,uint32_t value);
	uint32_t readReg(uint32_t index);

	void writeBridge(uint32_t index, const std::array<uint64_t, 8> &value);
	std::array<uint64_t, 8> readBridge(uint32_t index);
    void writeBridge(uint32_t index, uint64_t *value);
    void readBridge(uint32_t index, uint64_t *value);
    void writeBridgeAligned(uint32_t index, uint64_t *value);
    void readBridgeAligned(uint32_t index, uint64_t *value);

	void* getBridgeAddr();
	void* getLiteAddr();

private:
	uint8_t pci_bus;
    size_t bridge_bar_size;
private:
    volatile uint32_t *config_bar{};
	volatile uint32_t *lite_bar{};
	volatile __m512i *bridge_bar{};
};



#endif
#ifndef _QDMACONTROLLER_HPP_
#define _QDMACONTROLLER_HPP_

#include <cstdint>
#include <immintrin.h>

class FPGAController{
public:

private:
    volatile uint32_t *config_bar;
	volatile uint32_t *lite_bar;
	volatile __m512i *bridge_bar;
};

#endif
#ifndef _TIMINGFUNCTIONS_H_
#define _TIMINGFUNCTIONS_H_

#include <stdint.h>
#include <stddef.h>

struct TimingResult {
	uint32_t eClocks;
	size_t actualLength;
};

struct TimingResult TimeConsequtiveReads(__reg("a2") volatile uint8_t *timerLowAddr, __reg("a0") void *readStartAddr, __reg("d0") size_t length); 

#endif

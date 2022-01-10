#ifndef _CIA_TIMER_H_
#define _CIA_TIMER_H_

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/interrupts.h>

#include <hardware/cia.h>

#include <stdint.h>

struct CiaTimer {
	struct ExecBase *sysBase;
	struct Library *resource;
	struct Interrupt *interrupt;
	struct CIA *cia;
	struct CiaTimerRegisters {
		volatile uint8_t *lowByte;
		volatile uint8_t *highByte;
		volatile uint8_t *control;
	} registers;
	UWORD number;
};

struct CiaTimer *AllocCiaTimer(struct ExecBase *SysBase, struct CiaTimer *ciaTimerStore);
void FreeCiaTimer(struct CiaTimer *ciaTimer);
void StartCiaTimer(struct CiaTimer *ciaTimer);
void PrintOwners(struct ExecBase *SysBase, struct DosLibrary *DOSBase);

#endif

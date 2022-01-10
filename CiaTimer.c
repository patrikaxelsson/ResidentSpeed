#include <proto/exec.h>
#include <proto/cia.h>

#include <stddef.h>

#include "CiaTimer.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

extern struct CIA ciaa;
extern struct CIA ciab;

static void DoNothing(void) {

}

static struct Interrupt *CreateInterrupt(struct ExecBase *SysBase, const char *name, void (*function)(__reg("a1") void *data), void *data) {
	struct Interrupt *interrupt = AllocMem(sizeof(*interrupt), MEMF_PUBLIC | MEMF_CLEAR);
	if(NULL != interrupt) {
		interrupt->is_Code = (void *) function;
		interrupt->is_Data = data;
		interrupt->is_Node.ln_Pri = 0;
		interrupt->is_Node.ln_Name = (void *) name;

		return interrupt;
	}
	return NULL;
}

static void FreeInterrupt(struct ExecBase *SysBase, struct Interrupt *interrupt) {
	if(NULL != interrupt) {
		FreeMem(interrupt, sizeof(struct Interrupt));
	}
}

// Allocate the first free CIA timer
struct CiaTimer *AllocCiaTimer(struct ExecBase *SysBase, struct CiaTimer *ciaTimerStore) {
	if (NULL == ciaTimerStore) {
		return NULL;
	}

	const struct CiaInfo {
		char *resourceName;
		struct CIA *cia;
	} ciaInfo[] = {
		{
			"ciaa.resource",
			&ciaa
		},
		{
			"ciab.resource",
			&ciab
		}
	};

	struct Interrupt *timerInterrupt = CreateInterrupt(SysBase, "CiaTimer", (void *) &DoNothing, NULL);
	if (NULL != timerInterrupt) {
		for (size_t infoIndex = 0; infoIndex < ARRAY_SIZE(ciaInfo); infoIndex++) {
			const char *resourceName = ciaInfo[infoIndex].resourceName;
			struct CIA *cia = ciaInfo[infoIndex].cia;
			struct Library *ciaResource = OpenResource((void *) resourceName);
			if (NULL != ciaResource) {
				for (unsigned timerNum = 0; timerNum < 2; timerNum++) {
					struct Interrupt *interrupt = AddICRVector(ciaResource, timerNum, timerInterrupt);
					if (NULL == interrupt) {
						*ciaTimerStore = (struct CiaTimer) {
							.sysBase = SysBase,
							.resource = ciaResource,
							.interrupt = timerInterrupt,
							.cia = cia,
							.registers = (struct CiaTimerRegisters) {
								.lowByte = ((uint8_t *) cia) + 0x400 + 0x200 * timerNum,
								.highByte = ((uint8_t *) cia) + 0x500 + 0x200 * timerNum,
								.control = ((uint8_t *) cia) + 0xe00 + 0x100 * timerNum
							},
							.number = timerNum
						};
						return ciaTimerStore;
					}
				}
			}
		}
	}

	FreeInterrupt(SysBase, timerInterrupt);

	return NULL;
}

void FreeCiaTimer(struct CiaTimer *ciaTimer) {
	if (NULL != ciaTimer) {
		RemICRVector(ciaTimer->resource, ciaTimer->number, ciaTimer->interrupt);
		FreeInterrupt(ciaTimer->sysBase, ciaTimer->interrupt);
	}
}

void StartCiaTimer(struct CiaTimer *ciaTimer) {
	*ciaTimer->registers.control = CIACRAF_START;
}

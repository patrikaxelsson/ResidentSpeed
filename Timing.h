#ifndef _TIMING_H_
#define _TIMING_H_

#include <devices/timer.h>
#include <exec/execbase.h>

#include <stdint.h>

struct timerequest *OpenTimerDevice(struct ExecBase *SysBase);
void CloseTimerDevice(struct ExecBase *SysBase, struct timerequest *timerReq);
/*uint64_t GetEClocks(struct Library *TimerBase);
uint64_t EClocksToMicros(struct Library *TimerBase, const uint64_t eClocks);*/

#endif // _TIMING_H_

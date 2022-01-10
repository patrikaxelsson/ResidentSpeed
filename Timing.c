#include <exec/exec.h>
#include <dos/dosextens.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include "Timing.h"

struct timerequest *OpenTimerDevice(struct ExecBase *SysBase) {
	struct MsgPort *timerReplyPort = CreateMsgPort();
	struct timerequest *timerReq = (struct timerequest *) CreateIORequest(timerReplyPort, sizeof(struct timerequest));
	if (NULL == timerReq || 0 != OpenDevice("timer.device", UNIT_ECLOCK, (struct IORequest *) timerReq, 0)) {
		DeleteIORequest((struct IORequest *) timerReq);
		DeleteMsgPort(timerReplyPort);
		timerReq = NULL;
	}
	return timerReq;
}

void CloseTimerDevice(struct ExecBase *SysBase, struct timerequest *timerReq) {
	if (NULL != timerReq) {
		CloseDevice((struct IORequest *) timerReq);
		struct MsgPort *timerReplyPort = timerReq->tr_node.io_Message.mn_ReplyPort;
		DeleteIORequest((struct timerequest *) timerReq);
		DeleteMsgPort(timerReplyPort);
	}
}

static uint32_t GetEClockFreq(struct Library *TimerBase) {
	struct EClockVal dummyEClockVal;
	return ReadEClock(&dummyEClockVal);
}

/*uint64_t GetEClocks(struct Library *TimerBase) {
	struct EClockVal eClockVal;
	ReadEClock(&eClockVal);
	return (uint64_t) eClockVal.ev_hi << 32 | eClockVal.ev_lo;
}

uint64_t EClocksToMicros(struct Library *TimerBase, const uint64_t eClocks) {
	const uint32_t eClockFreq = GetEClockFreq(TimerBase);
	return (eClocks * 1000000) / eClockFreq;
}*/

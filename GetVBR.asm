		opt	p=68010

		xdef _GetVBRInSupervisorMode

_GetVBRInSupervisorMode:
	movec.l	VBR,d0
	rte


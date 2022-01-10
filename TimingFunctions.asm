		xdef _TimeConsequtiveReads
		xdef @TimeConsequtiveReads


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw48x:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	a2,a1

	move.l	a0,-(sp)	; Save start address for later.
	divu.w	#48,d0		; The resolution of each step.
	swap	d0			; To not have to worry about upper bytes
	clr.w	d0			; being populated with the remainder in
	swap	d0			; consuming functions.
	move.w	d0,d1		; Calculate jump offset for
	and.w	#7,d1		; how many 48B steps
	moveq	#8,d2		; in to start the initial run
	sub.w	d1,d2		; of the the read loop at.
	mulu.w	#4,d2		; Multiply with sizeof movem.l, how to calc that?
	lea		.loopStart(pc,d2.w),a2
	lsr.w	#3,d0		; Calculate how many times to run the loop.
						; We don't need any special handling of zero here
						; as if length is zero, we will jump to the end of
						; the loop.

	move.b	(a1),-(sp)	; Save timer before value.
	jmp		(a2)
.loopStart:
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
	movem.l	(a0)+,d1-d7/a2-a6
.loopEnd:
	dbra	d0,.loopStart

	move.b	(a1),d1	; Get timer after value.
	clr.w	d0
	move.b	(sp)+,d0	; Restore timer before value.
	sub.b	d1,d0		; Calculate duration of test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	(sp)+,d1	; data length;

	movem.l	(sp)+,d2-d7/a2-a6
	rts


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw4x:
	movem.l	d2/a3,-(sp)

	move.l	a0,a3		; Save start address for later.

	lsr.l	#2,d0		; Calculate how many longwords to read.
	move.w	d0,d1		; Calculate how many steps into
	and.w	#15,d1		; the loop we should start at
	moveq	#16,d2		; for the first iteration.
	sub.w	d1,d2
	mulu.w	#2,d2		; Multiply with sizeof move.l to get right offset,
						; (how to calc that properly)?
	lea		.loopStart(pc,d2.w),a1

	move.l	d0,d1
	lsr.l	#4,d1		; How many times to loop

	moveq	#0,d0
	move.b	(a2),d0		; Save timer before value.
	jmp		(a1)
.loopStart:
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
	move.l	(a0)+,d2
.loopEnd:
	dbra	d1,.loopStart

	sub.b	(a2),d0		; Get timer after value and calculate duration of
						; test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	a3,d1		; data length.

	movem.l	(sp)+,d2/a3
	rts


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of test
; Returns:
; d0 - diff
; d1 - actual length read
_TimeConsequtiveReads:
@TimeConsequtiveReads:
	move.l	a0,d1	; Align address upward to even
	add.l	#3,d1	; longword to avoid unaligned
	lsr.l	#2,d1   ; move.l's which gives slowdown
	lsl.l	#2,d1   ; on 020+ and this would only
	move.l	d1,a0	; be confusing.

;	bsr.s	TimeConsequtiveReadsRaw4x
 	cmp.l	#96,d0
 	blt.s	.lessThan96
 	bsr.w	TimeConsequtiveReadsRaw48x
 	bra.s	.afterTiming
 .lessThan96:
 	cmp.l	#48,d0
 	blt.s	.lessThan48
 	bsr.w	TimeConsequtiveReadsRaw48
 	bra.s	.afterTiming
 .lessThan48:
 	cmp.l	#40,d0
 	blt.s	.lessThan40
 	bsr.w	TimeConsequtiveReadsRaw40
 	bra.s	.afterTiming
 .lessThan40:
 	cmp.l	#32,d0
 	blt.s	.lessThan32
 	bsr.w	TimeConsequtiveReadsRaw32
 	bra.s	.afterTiming
 .lessThan32:
 	cmp.l	#24,d0
 	blt.s	.noTiming
 	bsr.w	TimeConsequtiveReadsRaw24
 	bra.s	.afterTiming
 .noTiming:
 	clr.l	d0
 	clr.l	d1

.afterTiming:
	tst.b	d0		; The uncompensated timing can only be zero if the test
	beq.s	.end	; has not been run, so return as is in that case.
	move.l	d1,-(sp)	; Save actual length.
	move.l	d0,-(sp)	; Save duration.

	bsr.s	CalcCyclesPerTimerRead
	move.l	d0,d1
	move.l	(sp)+,d0	; Restore duration.
	sub.b	d1,d0		; Apply cycle cost of timer register access. 
	move.l	(sp)+,d1	; Restore actual length;

.end:
	rts


; Arguments:
; a2 = CIA timer low byte addr
; Returns:
; d0.b cycles per read
CalcCyclesPerTimerRead:
	clr.l	d0	; To not have to worry about upper bytes being populated
				; with garbage in consumer functions.

	move.b	(a2),d0	; Save before-value of the decreasing timer register
	tst.b	(a2)
	tst.b	(a2)
	tst.b	(a2)
	tst.b	(a2)
	tst.b	(a2)
	tst.b	(a2)
	tst.b	(a2)
	sub.b	(a2),d0	; Subtract value of last read to get number of E
					; clock cycles needed for eight reads.
	lsr.b	#3,d0	; Calc E clocks cycles needed per read. 
	rts


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw48:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	a2,a1

	move.l	a0,-(sp)	; Save start address for later.

	clr.l	d0			; To not have to worry about upper bytes being populated
						; with garbage in consumer functions.
	move.b	(a1),d0		; Save timer before value.

	movem.l	(a0)+,d1-d7/a2-a6

	sub.b	(a1),d0 	; Get timer after value and calculate duration of test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	(sp)+,d1	; data length;

	movem.l	(sp)+,d2-d7/a2-a6
	rts


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw40:
	movem.l	d2-d7/a3-a6,-(sp)

	move.l	a0,-(sp)	; Save start address for later.

	clr.l	d0			; To not have to worry about upper bytes being populated
						; with garbage in consumer functions.
	move.b	(a2),d0		; Save timer before value.

	movem.l	(a0)+,d2-d7/a3-a6

	sub.b	(a2),d0 	; Get timer after value and calculate duration of test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	(sp)+,d1	; data length;

	movem.l	(sp)+,d2-d7/a3-a6
	rts



; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw32:
	movem.l	d4-d7/a3-a6,-(sp)

	move.l	a0,-(sp)	; Save start address for later.

	clr.l	d0			; To not have to worry about upper bytes being populated
						; with garbage in consumer functions.
	move.b	(a2),d0		; Save timer before value.

	movem.l	(a0)+,d4-d7/a3-a6

	sub.b	(a2),d0 	; Get timer after value and calculate duration of test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	(sp)+,d1	; data length;

	movem.l	(sp)+,d4-d7/a3-a6
	rts


; Arguments:
; a2 = CIA timer low addr
; a0 = read start addr
; d0 = length of read
; Returns:
; d0 - diff (cost of timer register access not accounted for)
; d1 - actual length read
TimeConsequtiveReadsRaw24:
	movem.l	d6-d7/a3-a6,-(sp)

	move.l	a0,-(sp)	; Save start address for later.

	clr.l	d0			; To not have to worry about upper bytes being populated
						; with garbage in consumer functions.
	move.b	(a2),d0		; Save timer before value.

	movem.l	(a0)+,d6-d7/a3-a6

	sub.b	(a2),d0 	; Get timer after value and calculate duration of test.

.end:
	move.l	a0,d1		; Calculate actual read
	sub.l	(sp)+,d1	; data length;

	movem.l	(sp)+,d6-d7/a3-a6
	rts


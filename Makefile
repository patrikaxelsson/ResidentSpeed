all: TestResidentSpeed

GetVBR.o: GetVBR.asm
	vasmm68k_mot -m68020 -Fhunk -o $@ $<

TestResidentSpeed: TestResidentSpeed.c CiaTimer.c GetVBR.o TimingFunctions.asm CiaTimer.h Makefile
	vc +aos68k -nostdlib -c99 -O1 -sc -lvc -lamiga -D__NOLIBBASE__ -o $@ $< CiaTimer.c GetVBR.o TimingFunctions.asm

clean:
	$(RM) TestResidentSpeed GetVBR.o

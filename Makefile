all: TestResidentSpeed

TestResidentSpeed: TestResidentSpeed.c CiaTimer.c TimingFunctions.asm CiaTimer.h Makefile
	vc +aos68k -nostdlib -c99 -O1 -sc -lvc -lamiga -D__NOLIBBASE__ -o $@ $< CiaTimer.c TimingFunctions.asm

clean:
	$(RM) TestResidentSpeed

all: ResidentSpeed

ResidentSpeed: ResidentSpeed.c CiaTimer.c GetVBR.asm TimingFunctions.asm CiaTimer.h Makefile
	vc +aos68k -nostdlib -O1 -size -sc -D__NOLIBBASE__ -lvc -lamiga -o $@ $< CiaTimer.c GetVBR.asm TimingFunctions.asm

clean:
	$(RM) ResidentSpeed

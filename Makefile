# PS2 Game Boy Emulator Makefile
# Requires PS2DEV SDK (ps2dev/ps2sdk)

EE_BIN = ps2gb.elf
EE_BIN_PKD = ps2gb_packed.elf

EE_OBJS = src/main.o \
          src/gb.o \
          src/cpu.o \
          src/mmu.o \
          src/ppu.o \
          src/display.o \
          src/rom_data.o

EE_CFLAGS = -O2 -Wall -G0
EE_LDFLAGS = -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib

EE_LIBS = -lgsKit -lgsToolkit -ldmaKit \
          -lpadx -lpatches \
          -lc -lkernel -ldebug

EE_INCS = -I$(PS2SDK)/ee/include \
          -I$(PS2SDK)/common/include \
          -I$(PS2DEV)/gsKit/include \
          -Isrc

all: $(EE_BIN) pack

pack: $(EE_BIN)
	ps2-packer $(EE_BIN) $(EE_BIN_PKD)

clean:
	rm -f $(EE_OBJS) $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

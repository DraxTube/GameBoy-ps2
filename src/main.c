#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "gb.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "input.h"
#include "display.h"

/* Embedded test ROM (Tetris bootleg / open source test) */
/* Replace with actual ROM bytes or load from memory card */
extern u8 rom_data[];
extern u32 rom_size;

static GB_State gb;
static GSGLOBAL *gsGlobal;

/* PS2 pad buffer */
static u8 pad_buf[256] __attribute__((aligned(64)));
static u8 pad_buf2[256] __attribute__((aligned(64)));

void init_ps2(void) {
    SifInitRpc(0);
    
    /* Init GS */
    gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACE_ON;
    gsGlobal->Field = GS_FIELD_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PSM = GS_PSM_CT16;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);
    
    /* Init pad */
    padInit(0);
    padPortOpen(0, 0, pad_buf);
    padPortOpen(1, 0, pad_buf2);
    
    printf("PS2 GB Emulator - Init OK\n");
}

int main(int argc, char *argv[]) {
    init_ps2();
    
    /* Init Game Boy */
    gb_init(&gb);
    
    /* Load ROM */
    if (gb_load_rom(&gb, rom_data, rom_size) != 0) {
        printf("Failed to load ROM!\n");
        /* Try to load from mass: */
        /* gb_load_rom_file(&gb, "mass:/gb/game.gb"); */
        SleepThread();
    }
    
    /* Init display subsystem */
    display_init(gsGlobal);
    
    printf("Starting emulation...\n");
    
    /* Main loop */
    while (1) {
        /* Read PS2 pad input */
        struct padButtonStatus pad_status;
        u32 buttons = 0xFFFF;
        
        if (padRead(0, 0, &pad_status) != 0) {
            buttons = 0xFFFF ^ pad_status.btns;
        }
        
        /* Map PS2 buttons to GB buttons */
        gb_update_input(&gb, buttons);
        
        /* Run one frame worth of cycles */
        /* GB runs at ~4.19 MHz, 70224 cycles per frame */
        int cycles_per_frame = 70224;
        while (cycles_per_frame > 0) {
            int cycles = cpu_step(&gb);
            ppu_step(&gb, cycles);
            timer_step(&gb, cycles);
            cycles_per_frame -= cycles;
        }
        
        /* Render frame */
        display_render(gsGlobal, gb.ppu.framebuffer);
        
        /* VSync */
        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }
    
    return 0;
}

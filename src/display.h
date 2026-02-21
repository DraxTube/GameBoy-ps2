#ifndef DISPLAY_H
#define DISPLAY_H

#include <gsKit.h>
#include "gb.h"

void display_init(GSGLOBAL *gsGlobal);
void display_render(GSGLOBAL *gsGlobal, const u8 *framebuffer);

#endif

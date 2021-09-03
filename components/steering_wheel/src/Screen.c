#include "Screen.h"
#include "EVE.h"

/* some pre-definded colors */
#define RED		0xff0000UL
#define ORANGE	0xffa500UL
#define GREEN	0x00ff00UL
#define BLUE	0x0000ffUL
#define BLUE_1	0x5dade2L
#define YELLOW	0xffff00UL
#define PINK	0xff00ffUL
#define PURPLE	0x800080UL
#define WHITE	0xffffffUL
#define BLACK	0x000000UL

#define LAYOUT_Y1 66
#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096) /* 0xff000 - start-address of the static part of the display-list, upper 4k of gfx-mem */

uint32_t num_dl_static; /* amount of bytes in the static part of our display-list */

void initStaticBackground(void)
{
    EVE_memWrite8(REG_PWM_DUTY, 0x30);	/* setup backlight, range is from 0 = off to 0x80 = max */
    EVE_cmd_dl(CMD_DLSTART); // tells EVE to start a new display-list
    EVE_cmd_dl(DL_CLEAR_RGB | WHITE); // sets the background color
    EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
    EVE_color_rgb(BLACK);
    EVE_cmd_text(5, 15, 28, 0, "Hello there!");
    EVE_cmd_dl(DL_DISPLAY); // put in the display list to mark its end
    EVE_cmd_dl(CMD_SWAP); // tell EVE to use the new display list
}

#if 0
void initStaticBackground(void)
{
    EVE_memWrite8(REG_PWM_DUTY, 0x30);	/* setup backlight, range is from 0 = off to 0x80 = max */
	EVE_cmd_dl(CMD_DLSTART); /* Start the display list */

	EVE_cmd_dl(TAG(0)); /* do not use the following objects for touch-detection */

	EVE_cmd_bgcolor(0x00c0c0c0); /* light grey */

	EVE_cmd_dl(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */

	/* draw a rectangle on top */
	EVE_cmd_dl(DL_BEGIN | EVE_RECTS);
	EVE_cmd_dl(LINE_WIDTH(1*16)); /* size is in 1/16 pixel */

	EVE_cmd_dl(DL_COLOR_RGB | BLUE_1);
	EVE_cmd_dl(VERTEX2F(0,0));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE,LAYOUT_Y1-2));
	EVE_cmd_dl(DL_END);

	/* draw a black line to separate things */
	EVE_cmd_dl(DL_COLOR_RGB | BLACK);
	EVE_cmd_dl(DL_BEGIN | EVE_LINES);
	EVE_cmd_dl(VERTEX2F(0,LAYOUT_Y1-2));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE,LAYOUT_Y1-2));
	EVE_cmd_dl(DL_END);

	EVE_cmd_text(EVE_HSIZE/2, 15, 29, EVE_OPT_CENTERX, "EVE Demo");

	EVE_cmd_text(10, EVE_VSIZE - 50, 26, 0, "DL-size:");
	EVE_cmd_text(10, EVE_VSIZE - 35, 26, 0, "Time1:");
	EVE_cmd_text(10, EVE_VSIZE - 20, 26, 0, "Time2:");

	EVE_cmd_text(125, EVE_VSIZE - 35, 26, 0, "us");
	EVE_cmd_text(125, EVE_VSIZE - 20, 26, 0, "us");

    EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
    EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */

	while (EVE_busy());

	// num_dl_static = EVE_memRead16(REG_CMD_DL);

	// EVE_cmd_memcpy(MEM_DL_STATIC, EVE_RAM_DL, num_dl_static);
	// while (EVE_busy());
}
#endif

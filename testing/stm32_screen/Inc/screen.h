#pragma once

#include "FT_NHD_43RTP_SHIELD.h"

extern FT800IMPL_SPI SCR;

void make_pill(int x, int y) {  // coords are in the form {x, y}
	SCR.PointSize(246);
	SCR.Begin(FT_POINTS);
	SCR.Vertex2ii(x - 25, y, 1, 0);
	SCR.Vertex2ii(x + 25, y, 1, 0);
	SCR.End();
	SCR.Begin(FT_RECTS);
	SCR.Vertex2ii(x - 25, y - 15, 0, 0);
	SCR.Vertex2ii(x + 25, y + 15, 0, 0);
	SCR.End();
}

// display elements that are shown on most pages
void common_display() {
	SCR.PointSize(160);
	SCR.Begin(FT_POINTS);

	uint32_t col;

	col = 0x00FF00;
	SCR.ColorRGB(col);
	SCR.Vertex2ii(20, 35, 1, 0);

	col = 0x00FF00;
	SCR.ColorRGB(col);
	SCR.Vertex2ii(445, 35, 1, 0);

	col = 0x00FF00;
	SCR.ColorRGB(col);
	SCR.Vertex2ii(150, 255, 1, 0);

	col = 0x00FF00;
	SCR.ColorRGB(col);
	SCR.Vertex2ii(210, 255, 1, 0);

	col = 0x00FF00;
	SCR.ColorRGB(col);
	SCR.Vertex2ii(270, 255, 1, 0);
	SCR.End();


	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(20, 15, 21, FT_OPT_CENTER, "DRS");

	SCR.ColorRGB(0xFFFFF);
	SCR.Cmd_Text(445, 15, 21, FT_OPT_CENTER, "LC");

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(150, 235, 21, FT_OPT_CENTER, "LC");

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(210, 235, 21, FT_OPT_CENTER, "AS");

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(270, 237, 21, FT_OPT_CENTER, "CTRL");


	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(330, 237, 21, FT_OPT_CENTER, "WC");

	const char *d_w = "D";
	SCR.Cmd_Text(330, 255, 28, FT_OPT_CENTER, d_w);


	if (true) {
		SCR.ColorRGB(255, 0, 0);
		SCR.Cmd_Text(240, 208, 27, FT_OPT_CENTER, "IGN");
	}
}


void main_display() {
	uint32_t col = 0;
	int x = 0;
	int y = 0;

	// Top Left Pill (Battery Voltage)
    col = 0x00FF00;

	x = 45;
	y = 105;
	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "B_Volt");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-15, y, 28, FT_OPT_CENTER, 13);
	SCR.Cmd_Text(x, y, 28, FT_OPT_CENTER, ".");
	SCR.Cmd_Number(x+7, y, 28, FT_OPT_CENTER, 1);
	SCR.Cmd_Text(x+27, y, 28, FT_OPT_CENTER, "V");


	// Top Right Pill (Coolant Temp)
    col = 0x00FF00;

	x = 435;
	y = 105;

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "C_Temp");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-15, y, 28, FT_OPT_SIGNED | FT_OPT_CENTER, 23);
	SCR.Cmd_Text(x+27, y, 28, FT_OPT_CENTER, "C");
	SCR.Cmd_Text(x+17, y-10, 26, FT_OPT_CENTER, "o");


	// Middle Left Pill (Oil Pressure)
	x = 45;
	y = 170;
    col = 0x00FF00;

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "O_Press");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-15, y, 28, FT_OPT_SIGNED | FT_OPT_CENTER, 100);
	SCR.Cmd_Text(x+27, y, 28, FT_OPT_CENTER, "C");
	SCR.Cmd_Text(x+17, y-8, 26, FT_OPT_CENTER, "o");


	// Middle Right Pill (Fuel Pressure)
    col = 0x00FF00;

	x = 435;
	y = 170;
	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "F_Press");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-21, y, 28, FT_OPT_SIGNED | FT_OPT_CENTER, 120);
	SCR.Cmd_Text(x+18, y, 28, FT_OPT_CENTER, "kPa");


	// Bottom Left Pill (Oil Temp)
    col = 0x00FF00;

	x = 45;
	y = 235;

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "O_Temp");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-15, y, 28, FT_OPT_SIGNED | FT_OPT_CENTER, 40);
	SCR.Cmd_Text(x+27, y, 28, FT_OPT_CENTER, "C");
	SCR.Cmd_Text(x+17, y-8, 26, FT_OPT_CENTER, "o");


	// Bottom Right Pill (Fuel Temp)
	x = 435;
	y = 235;
    col = 0x00FF00;

	SCR.ColorRGB(0xFFFFFF);
	SCR.Cmd_Text(x, y-25, 26, FT_OPT_CENTER, "F_Temp");
	SCR.ColorRGB(col);
	make_pill(x, y);
	SCR.ColorRGB(0, 0, 0);
	SCR.Cmd_Number(x-15, y, 28, FT_OPT_SIGNED | FT_OPT_CENTER, 24);
	SCR.Cmd_Text(x+27, y, 28, FT_OPT_CENTER, "C");
	SCR.Cmd_Text(x+17, y-8, 26, FT_OPT_CENTER, "o");

	SCR.ColorRGB(255, 255, 255);
	SCR.Cmd_Number(240, 45, 28, FT_OPT_CENTER, 12000);

	const char * gears[6]= {"N", "1", "2", "3", "4", "5"};
    int8_t gear = 3;
	if(gear > -1 && gear < 7) {
		SCR.Cmd_Text(240, 156, 0, FT_OPT_CENTER, gears[gear]);
	} else {
		SCR.Cmd_Text(240, 156, 28, FT_OPT_CENTER, "?");
	}

	common_display();
}


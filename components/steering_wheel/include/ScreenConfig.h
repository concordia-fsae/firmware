/*
 * Config file for the touch screen
 */

#pragma once

// choose NHD 4.3 here, specific configs are in EVE_config.h
// #define EVE_NHD_43

// #define Resolution_480x272

#define EVE_HAS_AUDIO false

// display settings
#define EVE_HAS_PDN        false
#define EVE_HSIZE          (480L) /* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_VSIZE          (272L) /* Tvd Number of visible lines (in lines) - display height */
#define EVE_PCLK           (6L)
#define EVE_PCLKPOL        (1L)
#define EVE_SWIZZLE        (0L)
#define EVE_CSPREAD        (1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL    true
#define EVE_GEN 2

// Timing
#define EVE_VSYNC0  (0L)   /* Tvf Vertical Front Porch */
#define EVE_VSYNC1  (10L)  /* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET (12L)  /* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE  (292L) /* Tv Total number of lines (visible and non-visible) (in lines) */
#define EVE_HSYNC0  (0L)   /* (40L)	// Thf Horizontal Front Porch */
#define EVE_HSYNC1  (41L)  /* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET (43L)  /* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE  (548L) /* Th Total length of line (visible and non-visible) (in PCLKs) */

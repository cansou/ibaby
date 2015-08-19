/*
 * THER OLED DISPLAY
 *
 * Copyright (c) 2015 by Leo Liu <59089403@qq.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 *
 * 2015/06/01 - Init version
 *              by Leo Liu <59089403@qq.com>
 *
 */


#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "OSAL_Clock.h"
#include "OSAL_PwrMgr.h"

#include "config.h"
#include "ther_uart.h"

#include "thermometer.h"
#include "ther_oled9639_display.h"
#include "ther_oled9639_drv.h"

#define MODULE "[DISP   ] "

enum {

	STATE_POWER_OFF,

	STATE_INIT_VDD_VCC,
	STATE_INIT_DEVICE,
	STATE_DISPLAY_ON,
	STATE_DISPLAY_END,

	STATE_EXIT_VCC,
	STATE_EXIT_VDD,
};

static const char *state[] = {
	"power off",
	"init vddvcc",
	"init device",
	"display on",
	"display end",
	"exit vcc",
	"exit vdd",
};

#define DISPLAY_10MS 10 /* ms */
#define DISPLAY_INIT_VCC_VDD_TIME 30 /* ms */
#define DISPLAY_INIT_DEVICE_TIME 100 /* ms */
#define DISPLAY_EXIT_VCC_TIME 100 /* ms */

struct oled_display {
	uint8 task_id;

	uint8 cur_state;
	bool powering_off;

	uint8 picture;
	uint16 remain_ms;

	/* first picture param */
	bool bt_link;
	unsigned short temp;
	uint16 max_temp;
	unsigned short time;
	unsigned char batt_percentage;

	/* second picture param */

	/* report event to ther */
	void (*event_report)(unsigned char event, unsigned short param);
};

static struct oled_display display;

/* Time */
#define TIME_HOUR_1_START_COL 1
#define TIME_HOUR_1_END_COL 11
#define TIME_HOUR_2_START_COL 11
#define TIME_HOUR_2_END_COL 21

#define TIME_COLON_START_COL 22
#define TIME_COLON_END_COL 28

#define TIME_MIN_1_START_COL 29
#define TIME_MIN_1_END_COL 39
#define TIME_MIN_2_START_COL 39
#define TIME_MIN_2_END_COL 49


/* BT link*/
#define BLUETOOTH_START_COL 62
#define BLUETOOTH_END_COL 72

/* Batt */
#define BATT_START_COL 74
#define BATT_END_COL 94

/* Temp */
#define TEMP_1_START_COL 10
#define TEMP_1_END_COL 23
#define TEMP_2_START_COL 23
#define TEMP_2_END_COL 36

#define TEMP_DOT_START_COL 37
#define TEMP_DOT_END_COL 45

#define TEMP_3_START_COL 46
#define TEMP_3_END_COL 59
/*
#define TEMP_4_START_COL 59
#define TEMP_4_END_COL 72
*/

#define TEMP_CELSIUS_START_COL 62
#define TEMP_CELSIUS_END_COL 82

/* MAX text */
#define MAX_TEXT_START_COL 1
#define MAX_TEXT_END_COL 39

/* MAX text colon */
#define MAX_TEXT_COLON_START_COL 39
#define MAX_TEXT_COLON_END_COL 45

/* MAX Temp */
#define MAX_TEMP_1_START_COL 46
#define MAX_TEMP_1_END_COL 59
#define MAX_TEMP_2_START_COL 59
#define MAX_TEMP_2_END_COL 72

#define MAX_TEMP_DOT_START_COL 72
#define MAX_TEMP_DOT_END_COL 80

#define MAX_TEMP_3_START_COL 80
#define MAX_TEMP_3_END_COL 93

/*
#define MAX_TEMP_4_START_COL x
#define MAX_TEMP_4_END_COL x
*/

/*
 * ROW * COL
 */

static const unsigned char welcome_24x90[] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x7F, 0xFE, 0xF0, 0xC0, 0xF8, 0xFF, 0x1F, 0xFF, 0xF8, 0xC0, 0xE0, 0xFE, 0x7F, 0x0F, 0x00, 0x60, 0xFC, 0xFE, 0x3F, 0x37, 0x33, 0x37, 0x3F, 0x3E, 0x3C, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xF8, 0xFE, 0xFE, 0x07, 0x07, 0x03, 0x03, 0x07, 0x00, 0x00, 0xF8, 0xFE, 0xFF, 0x07, 0x03, 0x03, 0x07, 0x8F, 0xFE, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x0F, 0x07, 0x03, 0x07, 0xFF, 0xFE, 0xFE, 0x07, 0x07, 0x03, 0x07, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0xFC, 0xFE, 0xBF, 0x37, 0x33, 0x33, 0x3F, 0x3E, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x07, 0x07, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x07, 0x07, 0x07, 0x00, 0x00, 0x01, 0x03, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x01, 0x03, 0x07, 0x07, 0x06, 0x06, 0x06, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00
};

static const unsigned char goodbye_24x90[] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0xFE, 0xFF, 0x87, 0x03, 0x01, 0x01, 0x83, 0xFF, 0xFF, 0x00, 0x00, 0x30, 0xFE, 0xFF, 0x87, 0x03, 0x01, 0x01, 0x83, 0xFF, 0xFF, 0x7C, 0x00, 0x00, 0xFE, 0xFF, 0xC7, 0x03, 0x01, 0x01, 0x83, 0xFF, 0xFF, 0xFC, 0x00, 0x00, 0xFE, 0xFF, 0xC7, 0x03, 0x01, 0x01, 0x83, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x83, 0x01, 0x01, 0x03, 0xCF, 0xFF, 0xFE, 0x00, 0x01, 0x07, 0x3F, 0xFE, 0xF0, 0xC0, 0xF0, 0xFE, 0x3F, 0x07, 0x01, 0x00, 0xFE, 0xFF, 0xDF, 0x1B, 0x19, 0x1B, 0x1F, 0x1F, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x63, 0x63, 0x63, 0x63, 0x7B, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0x78, 0x7F, 0x1F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char battery_full_16_20[][40] = {
	0x00, 0x00, 0xE0, 0xF0, 0x10, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x08, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x08, 0x0F, 0x00
};

static const unsigned char battery_almost_full_16_20[][40] = {
	0x00, 0x00, 0xE0, 0xF0, 0x10, 0x10, 0x10, 0x10, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x08, 0x0F, 0x00
};

static const unsigned char battery_half_16_20[][40] = {
	0x00, 0x00, 0xE0, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x08, 0x0F, 0x00
};

static const unsigned char battery_almost_empty_16_20[][40] = {
	0x00, 0x00, 0xE0, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xD0, 0xD0, 0xD0, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x0B, 0x08, 0x0F, 0x00
};

static const unsigned char battery_empty_16_20[][40] = {
	0x00, 0x00, 0xE0, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0F, 0x00
};

static const unsigned char bluetooth_16_10[][20] = {
	0x00, 0x00, 0x10, 0x20, 0xFC, 0x88, 0x50, 0x20, 0x00, 0x00, 0x00, 0x00, 0x08, 0x04, 0x3F, 0x11, 0x0A, 0x04, 0x00, 0x00
};

static const unsigned char dummy_celsius_24x13[39] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char dot_24x8[][24] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0F, 0x0F, 0x06, 0x00, 0x00
};

static const unsigned char celsius_24x20[][60] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0xE0, 0xF0, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x00, 0x00
};

/*
 * Segoe UI Symbol 15
 */
static const uint8 max_text_24x38[] = {
	0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x0F, 0x3E, 0xF8, 0xE0, 0xE0, 0xF8, 0x3F, 0x07, 0xFF, 0xFF, 0x00, 0x80, 0xE0, 0xFC, 0x7F, 0x67, 0x6F, 0x7F, 0xF8, 0xC0, 0x00, 0x00, 0x83, 0xE7, 0xFE, 0x3C, 0xFE, 0xE7, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x01, 0x03, 0x03, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00
};

/*
 * :
 */
static const unsigned char colon_16x6[12] = {
	0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00
};

static const unsigned char number_16x10[][20] = {

	/* 0 */
	0x00, 0xF0, 0xF8, 0x04, 0x04, 0x04, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x20, 0x20, 0x20, 0x1F, 0x0F, 0x00, 0x00,

	/* 1 */
	0x00, 0x00, 0x10, 0x18, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,

	/* 2 */
	0x00, 0x10, 0x18, 0x0C, 0x0C, 0x0C, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x30, 0x38, 0x3C, 0x36, 0x33, 0x31, 0x30, 0x00, 0x00,

	/* 3 */
	0x00, 0x08, 0x8C, 0x8C, 0x8C, 0x8C, 0xFC, 0xF8, 0x00, 0x00, 0x00, 0x10, 0x31, 0x31, 0x31, 0x31, 0x3F, 0x1F, 0x00, 0x00,

	/* 4 */
	0x00, 0x80, 0xC0, 0x60, 0x30, 0x18, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0C, 0x0C, 0x0C, 0x3F, 0x3F, 0x0C, 0x00,

	/* 5 */
	0x00, 0xFC, 0xFC, 0xCC, 0xCC, 0xCC, 0xCC, 0x8C, 0x00, 0x00, 0x00, 0x18, 0x38, 0x30, 0x30, 0x30, 0x3F, 0x1F, 0x00, 0x00,

	/* 6 */
	0x00, 0xF8, 0xFC, 0x8C, 0x8C, 0x8C, 0x8C, 0x0C, 0x00, 0x00, 0x00, 0x1F, 0x3F, 0x31, 0x31, 0x31, 0x3F, 0x1F, 0x00, 0x00,

	/* 7 */
	0x00, 0x0C, 0x0C, 0x0C, 0x8C, 0xCC, 0xFC, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x01, 0x00, 0x00, 0x00, 0x00,

	/* 8 */
	0x00, 0xF8, 0xFC, 0x8C, 0x8C, 0x8C, 0x8C, 0xFC, 0xF8, 0x00, 0x00, 0x1E, 0x3F, 0x31, 0x31, 0x31, 0x31, 0x3F, 0x1E, 0x00,

	/* 9 */
	0x00, 0xF8, 0xFC, 0x8C, 0x8C, 0x8C, 0x8C, 0xFC, 0xF8, 0x00, 0x00, 0x18, 0x39, 0x31, 0x31, 0x31, 0x31, 0x3F, 0x1F, 0x00,
};

/*
 * Font 0 ~ 9
 *
 * 24 X 13 pix
 *
 * Lao UI 22
 */
static const unsigned char number_24x13[][39] = {
	/* 0 */
	0x00, 0x80, 0xC0, 0xE0, 0x70, 0x30, 0x30, 0x70, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0E, 0x1C, 0x18, 0x18, 0x1C, 0x0E, 0x07, 0x01, 0x00, 0x00,

	/* 1 */
	0x00, 0x00, 0xC0, 0xC0, 0xE0, 0xE0, 0xE0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F, 0x1F, 0x18, 0x18, 0x18, 0x00,

	/* 2 */
	0x00, 0x00, 0xC0, 0xE0, 0xE0, 0x70, 0x70, 0x70, 0x70, 0xE0, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0x78, 0x3F, 0x1F, 0x07, 0x00, 0x00, 0x00, 0x1E, 0x1F, 0x1F, 0x1F, 0x1D, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x00,

	/* 3 */
	0x00, 0x00, 0xE0, 0x60, 0x70, 0x70, 0x70, 0xE0, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x38, 0x38, 0x38, 0x7F, 0xFF, 0xE7, 0xC0, 0x00, 0x00, 0x00, 0x0C, 0x1C, 0x18, 0x18, 0x18, 0x1C, 0x1E, 0x0F, 0x07, 0x03, 0x00, 0x00,

	/* 4 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFE, 0xCF, 0xC7, 0xC1, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F, 0x1F, 0x1F, 0x01, 0x00,

	/* 5 */
	0x00, 0x00, 0xE0, 0xE0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x18, 0x18, 0x38, 0x38, 0xF8, 0xF0, 0xE0, 0x00, 0x00, 0x00, 0x0C, 0x1C, 0x1C, 0x18, 0x18, 0x1C, 0x1E, 0x0F, 0x07, 0x03, 0x00, 0x00,

	/* 6 */
	0x00, 0x00, 0x80, 0xC0, 0xE0, 0xE0, 0x70, 0x70, 0x70, 0x60, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x19, 0x18, 0x1C, 0x1C, 0x78, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x03, 0x0F, 0x0F, 0x1C, 0x18, 0x18, 0x1C, 0x1E, 0x0F, 0x07, 0x00, 0x00,

	/* 7 */
	0x00, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xFC, 0x7F, 0x1F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* 8 */
	0x00, 0x80, 0xE0, 0xE0, 0xF0, 0x70, 0x70, 0xF0, 0xE0, 0xE0, 0x80, 0x00, 0x00, 0x00, 0xC3, 0xEF, 0xFF, 0x7C, 0x38, 0x3C, 0x7C, 0xFF, 0xE7, 0xC1, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x1E, 0x1C, 0x18, 0x1C, 0x1C, 0x1F, 0x0F, 0x07, 0x00, 0x00,

	/* 9 */
	0x00, 0xC0, 0xE0, 0xE0, 0x60, 0x70, 0x70, 0xE0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x1F, 0x3F, 0x79, 0x70, 0x60, 0x60, 0x70, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x1C, 0x18, 0x1C, 0x1C, 0x0E, 0x0F, 0x07, 0x01, 0x00, 0x00,
};

static void show_time(bool show, unsigned short time)
{
	unsigned char hour = time >> 8;
	unsigned char minute = time & 0xFF;

	if (show) {
		oled_drv_write_block(0, 2, TIME_HOUR_1_START_COL, TIME_HOUR_1_END_COL, number_16x10[hour / 10]);
		oled_drv_write_block(0, 2, TIME_HOUR_2_START_COL, TIME_HOUR_2_END_COL, number_16x10[hour % 10]);

		oled_drv_write_block(0, 2, TIME_COLON_START_COL, TIME_COLON_END_COL, colon_16x6);

		oled_drv_write_block(0, 2, TIME_MIN_1_START_COL, TIME_MIN_1_END_COL, number_16x10[minute / 10]);
		oled_drv_write_block(0, 2, TIME_MIN_2_START_COL, TIME_MIN_2_END_COL, number_16x10[minute % 10]);
	} else {
		oled_drv_fill_block(0, 2, TIME_HOUR_1_START_COL, TIME_MIN_2_END_COL, 0);
	}
}



/*
 * 2771 => 27.71 celsius
 */
static void show_temp(bool show, uint16 temp)
{
	uint8 first_num, sec_num, third_num, forth_num;

	temp += 5; /* 4 she 5 ru*/

	first_num = temp / 1000;
	sec_num = (temp / 100) % 10;
	third_num = (temp / 10) % 10;
	forth_num = temp % 10;

	if (show) {
		oled_drv_write_block(2, 5, TEMP_1_START_COL, TEMP_1_END_COL, number_24x13[first_num % 10]);
		oled_drv_write_block(2, 5, TEMP_2_START_COL, TEMP_2_END_COL, number_24x13[sec_num]);

		oled_drv_write_block(2, 5, TEMP_DOT_START_COL, TEMP_DOT_END_COL, dot_24x8[0]);

		oled_drv_write_block(2, 5, TEMP_3_START_COL, TEMP_3_END_COL, number_24x13[third_num]);

		oled_drv_write_block(2, 5, TEMP_CELSIUS_START_COL, TEMP_CELSIUS_END_COL, celsius_24x20[0]);
	} else {
		oled_drv_fill_block(2, 5, TEMP_1_START_COL, TEMP_CELSIUS_END_COL, 0);
	}
}

static void show_batt(bool show, unsigned char batt_percentage)
{
	uint8 level;

	if (batt_percentage < 10)
		level = BATT_EMPTY;
	else if (batt_percentage < 40)
		level = BATT_ALMOST_EMPTY;
	else if (batt_percentage < 60)
		level = BATT_HALF;
	else if (batt_percentage < 80)
		level = BATT_ALMOST_FULL;
	else if (batt_percentage <= 100)
		level = BATT_FULL;
	else
		level = BATT_HALF;

	if (show) {
		switch (level) {
		case BATT_FULL:
			oled_drv_write_block(0, 2, BATT_START_COL, BATT_END_COL, battery_full_16_20[0]);
			break;

		case BATT_ALMOST_FULL:
			oled_drv_write_block(0, 2, BATT_START_COL, BATT_END_COL, battery_almost_full_16_20[0]);
			break;

		case BATT_HALF:
			oled_drv_write_block(0, 2, BATT_START_COL, BATT_END_COL, battery_half_16_20[0]);
			break;

		case BATT_ALMOST_EMPTY:
			oled_drv_write_block(0, 2, BATT_START_COL, BATT_END_COL, battery_almost_empty_16_20[0]);
			break;

		case BATT_EMPTY:
			oled_drv_write_block(0, 2, BATT_START_COL, BATT_END_COL, battery_empty_16_20[0]);
			break;

		default:
			break;
		}

	} else {
		oled_drv_fill_block(0, 2, BATT_START_COL, BATT_END_COL, 0);
	}
}

static void show_bt_link(bool show)
{
	if (show)
		oled_drv_write_block(0, 2, BLUETOOTH_START_COL, BLUETOOTH_END_COL, bluetooth_16_10[0]);
	else
		oled_drv_fill_block(0, 2, BLUETOOTH_START_COL, BLUETOOTH_END_COL, 0);
}


static void show_max_text(bool show)
{
	if (show) {
		oled_drv_write_block(1, 4, MAX_TEXT_START_COL, MAX_TEXT_END_COL, max_text_24x38);
	} else {
		oled_drv_fill_block(1, 4, MAX_TEXT_START_COL, MAX_TEXT_COLON_END_COL, 0);
	}
}


/*
 * 2770 => 27.70 celsius
 */
static void show_max_temp(bool show, uint16 temp)
{
	uint8 first_num, sec_num, third_num, forth_num;

	temp += 5; /* 4 she 5 ru*/

	first_num = temp / 1000;
	sec_num = (temp / 100) % 10;
	third_num = (temp / 10) % 10;
	forth_num = temp % 10;

	if (show) {
		oled_drv_write_block(1, 4, MAX_TEMP_1_START_COL, MAX_TEMP_1_END_COL, number_24x13[first_num % 10]);
		oled_drv_write_block(1, 4, MAX_TEMP_2_START_COL, MAX_TEMP_2_END_COL, number_24x13[sec_num]);

		oled_drv_write_block(1, 4, MAX_TEMP_DOT_START_COL, MAX_TEMP_DOT_END_COL, dot_24x8[0]);

		oled_drv_write_block(1, 4, MAX_TEMP_3_START_COL, MAX_TEMP_3_END_COL, number_24x13[third_num]);

	} else {
		oled_drv_fill_block(1, 4, MAX_TEMP_1_START_COL, MAX_TEMP_3_END_COL, 0);
	}
}


void oled_test(void)
{
	oled_drv_fill_block(0, 1, 0, MAX_COL, 0);
	oled_drv_fill_block(1, 2, 0, MAX_COL, 0xff);
	oled_drv_fill_block(2, 3, 0, MAX_COL, 0);
	oled_drv_fill_block(3, 4, 0, MAX_COL, 0xff);
	oled_drv_fill_block(4, MAX_PAGE, 0, MAX_COL, 0);
}

static void oled_display_draw_picture(struct oled_display *od)
{
	oled_drv_fill_block(0, MAX_PAGE, 0, MAX_COL, 0);

	switch (od->picture) {
	case OLED_PICTURE_WELCOME:
		oled_drv_write_block(1, 4, 3, 93, welcome_24x90);
		break;

	case OLED_PICTURE_GOODBYE:
		oled_drv_write_block(1, 4, 3, 93, goodbye_24x90);
		break;

	case OLED_PICTURE1:
		show_time(TRUE, od->time);

		show_bt_link(od->bt_link);

		show_batt(TRUE, od->batt_percentage);
		show_temp(TRUE, od->temp);

		break;

	case OLED_PICTURE2:
		show_max_text(TRUE);
		show_max_temp(TRUE, od->max_temp);

		break;

	default:
		print(LOG_WARNING, MODULE "unknown picture to shown!\n");
		break;
	}

	oled_drv_display_on();
}

void oled_update_picture(uint8 type, uint16 val)
{
	struct oled_display *od = &display;

	if (od->cur_state != STATE_DISPLAY_ON) {
		return;
	}

	switch (type) {
	case OLED_CONTENT_TIME:
		od->time = val;
		if (od->picture == OLED_PICTURE1)
			show_time(TRUE, od->time);
		break;

	case OLED_CONTENT_LINK:
		od->bt_link = val;
		if (od->picture == OLED_PICTURE1) {
			show_bt_link(od->bt_link);
		}
		break;

	case OLED_CONTENT_BATT:
		od->batt_percentage = val;
		if (od->picture == OLED_PICTURE1) {
			show_batt(TRUE, od->batt_percentage);
		}
		break;

	case OLED_CONTENT_TEMP:
		od->temp = val;
		if (od->picture == OLED_PICTURE1) {
			show_temp(TRUE, od->temp);
		}
		break;

	case OLED_CONTENT_MAX_TEMP:
		od->max_temp = val;
		if (od->picture == OLED_PICTURE2) {
			show_max_temp(TRUE, od->max_temp);
		}
		break;

	default:
		break;
	}
}

void oled_show_picture(uint8 picture, uint16 remain_ms, struct display_param *param)
{
	struct oled_display *od = &display;

	od->picture = picture;
	od->remain_ms = remain_ms;

	if (param) {
		od->bt_link = param->ble_link;
		od->temp = param->temp;
		od->time = param->time;
		od->batt_percentage = param->batt_percentage;
		od->max_temp = param->max_temp;
	}

	osal_stop_timerEx(od->task_id, TH_DISPLAY_EVT);
	osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_10MS);
}

void oled_show_next_picture(uint16 time_ms, struct display_param *param)
{
	struct oled_display *od = &display;

	oled_show_picture((od->picture + 1) % OLED_PICTURE_MAX, time_ms, param);
}


void oled_display_power_off(void)
{
	struct oled_display *od = &display;

	if (od->cur_state != STATE_POWER_OFF) {
		osal_stop_timerEx(od->task_id, TH_DISPLAY_EVT);

		od->powering_off = TRUE;
		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_10MS);
	}
}

static uint8 get_next_state(struct oled_display *od, uint8 cur_state)
{
	uint8 next_state;

	switch (cur_state) {
	case STATE_POWER_OFF:
		if (od->remain_ms != 0)
			next_state = STATE_INIT_VDD_VCC;
		break;

	case STATE_INIT_VDD_VCC:
		next_state = STATE_INIT_DEVICE;
		break;

	case STATE_INIT_DEVICE:
		next_state = STATE_DISPLAY_ON;
		break;

	case STATE_DISPLAY_ON:
//		if (od->powering_off) {
//			next_state = STATE_EXIT_VCC;
//		}

		if (od->remain_ms == 0)
			next_state = STATE_DISPLAY_END;
		else
			next_state = STATE_DISPLAY_ON;

		break;

	case STATE_DISPLAY_END:
		if (od->remain_ms == 0)
			next_state = STATE_EXIT_VCC;
		else
			next_state = STATE_DISPLAY_ON;
		break;

	case STATE_EXIT_VCC:
		next_state = STATE_EXIT_VDD;
		break;

	case STATE_EXIT_VDD:
		next_state = STATE_POWER_OFF;
		break;

	default:
		break;
	}

	return next_state;
}

#include "hal_i2c.h"

void oled_display_state_machine(void)
{
	struct oled_display *od = &display;

#ifdef DEBUG_OLED_DISPLAY
	print(LOG_DBG, MODULE "goto state <%s>\n",
			state[get_next_state(od, od->cur_state)]);
#endif

	od->cur_state = get_next_state(od, od->cur_state);

	switch (od->cur_state) {
	case STATE_POWER_OFF:
		break;

	case STATE_INIT_VDD_VCC:
		oled_drv_power_on_vdd();
//		oled_drv_power_on_vcc();

		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_INIT_VCC_VDD_TIME);
		break;

	case STATE_INIT_DEVICE:
		ther_wake_lock();

		oled_drv_init_device();

		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_INIT_DEVICE_TIME);
		break;

	case STATE_DISPLAY_ON:
		oled_display_draw_picture(od);

		od->event_report(OLED_EVENT_DISPLAY_ON, od->picture);
		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, od->remain_ms);
		od->remain_ms = 0;
		break;

	case STATE_DISPLAY_END:
		od->picture = OLED_PICTURE_NONE;
		od->event_report(OLED_EVENT_TIME_TO_END, od->picture);

		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_10MS);
		break;

	case STATE_EXIT_VCC:
		oled_drv_display_off();
		oled_drv_charge_pump_disable();
//		oled_drv_power_off_vcc();

		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_EXIT_VCC_TIME);
		break;

	case STATE_EXIT_VDD:
		oled_drv_power_off_vdd();
		oled_drv_exit();

		ther_wake_unlock();

		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_10MS);
		break;

	default:
		break;
	}

}

void oled_display_init(unsigned char task_id, void (*event_report)(unsigned char event, unsigned short param))
{
	struct oled_display *od = &display;

	print(LOG_INFO, MODULE "oled9639 display init\n");

	osal_memset(od, 0, sizeof(struct oled_display));

	od->task_id = task_id;
	od->event_report = event_report;
	od->cur_state = STATE_POWER_OFF;

	oled_drv_init();
}

void oled_display_exit(void)
{
	print(LOG_INFO, MODULE "oled9639 display exit\n");

	oled_drv_exit();
}

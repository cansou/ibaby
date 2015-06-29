
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "OSAL_Clock.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_oled9639_display.h"
#include "ther_oled9639_drv.h"

#define MODULE "[THER DISP] "

enum {

	STATE_POWER_OFF,
	STATE_VDD_ON,
	STATE_DISPLAY_ON,

	STATE_VCC_OFF,
	STATE_VDD_OFF,
};


#define DISPLAY_PREPARE_TIME 20 /* ms */
#define DISPLAY_DELAY_AFTER_VDD_ON 30 /* ms */
#define DISPLAY_DELAY_AFTER_VCC_ON 100 /* ms */
#define DISPLAY_DELAY_AFTER_VCC_OFF 100 /* ms */

struct oled_display {
	uint8 task_id;

	unsigned char state;

	bool powering_off;

	unsigned char picture;
	unsigned short remain_ms;

	/* first picture */
	bool ble_link;
	unsigned short temp;
	unsigned short time;
	unsigned char batt_level;

	/* second picture */

	void (*event_report)(unsigned char event, unsigned short param);
};

static struct oled_display display = {
	.state = STATE_POWER_OFF,
};

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

static const unsigned char battery_16_20[][40] = {
	0x00, 0x00, 0xF8, 0xFC, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0xF4, 0xF4, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x01, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x00
};

static const unsigned char bluetooth_16_10[][20] = {
		0x00, 0x04, 0x08, 0x10, 0x60, 0xFE, 0x84, 0x48, 0x30, 0x00, 0x00, 0x20, 0x10, 0x08, 0x06, 0x3F, 0x21, 0x12, 0x0C, 0x00
};

static const unsigned char dummy_celsius_24x13[39] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char dot_24x8[][24] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x00
};

static const unsigned char celsius_24x20[][60] = {
	0x00, 0x00, 0x30, 0x48, 0x48, 0x30, 0x00, 0xC0, 0xE0, 0x60, 0x30, 0x30, 0x30, 0x30, 0x30, 0x60, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x38, 0x1E, 0x0E, 0x00, 0x00
};

/*
 * :
 */
static const unsigned char colon_16x6[12] = {
	0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00
};

static const unsigned char number_16x10[][20] = {

	/* 0 */
	0x00, 0xF8, 0xFC, 0x0C, 0x0C, 0x0C, 0x0C, 0xFC, 0xF8, 0x00, 0x00, 0x1F, 0x3F, 0x30, 0x30, 0x30, 0x30, 0x3F, 0x1F, 0x00,

	/* 1 */
	0x00, 0x00, 0x20, 0x38, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00,

	/* 2 */
	0x00, 0x0C, 0x0E, 0x06, 0x06, 0x86, 0x86, 0xFE, 0xFC, 0x00, 0x00, 0x78, 0x7C, 0x6E, 0x67, 0x63, 0x61, 0x61, 0x60, 0x00,

	/* 3 */
	0x00, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xFE, 0x7C, 0x00, 0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x3F, 0x00,

	/* 4 */
	0x00, 0xE0, 0xF0, 0x18, 0x1C, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x06, 0x06, 0x7F, 0x7F, 0x06, 0x06, 0x00,

	/* 5 */
	0x00, 0xFC, 0xFC, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x00, 0x61, 0x63, 0x63, 0x63, 0x63, 0x63, 0x7F, 0x3F, 0x00,

	/* 6 */
	0x00, 0xF8, 0xFC, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x0C, 0x00, 0x00, 0x3F, 0x7F, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x3F, 0x00,

	/* 7 */
	0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0xFC, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x00,

	/* 8 */
	0x00, 0x00, 0x78, 0x84, 0x84, 0x84, 0x84, 0x78, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x21, 0x1E, 0x00, 0x00,

	/* 9 */
	0x00, 0xFC, 0xFE, 0x8E, 0x8E, 0x8E, 0x8E, 0xFE, 0xFC, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7F, 0x7F, 0x00,
};

/*
 * Font 0 ~ 9
 *
 * 24 X 13 pix
 */
static const unsigned char number_24x13[][39] = {
	/* 0 */
	0x00, 0x00, 0xE0, 0xF0, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1F, 0x0F, 0x00, 0x00,

	/* 1 */
	0x00, 0x00, 0x00, 0x20, 0x30, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x3F, 0x3F, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00,

	/* 2 */
	0x00, 0x00, 0x30, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0x70, 0x38, 0x18, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00,

	/* 3 */
	0x00, 0x00, 0x20, 0x30, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x08, 0x18, 0x30, 0x30, 0x30, 0x30, 0x38, 0x1F, 0x0F, 0x00, 0x00,

	/* 4 */
	0x00, 0x00, 0x00, 0x80, 0xC0, 0x60, 0x30, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,

	/* 5 */
	0x00, 0x00, 0xE0, 0xF0, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x38, 0x3F, 0x1F, 0x00, 0x00,

	/* 6 */
	0x00, 0x00, 0xC0, 0xE0, 0x70, 0x38, 0x18, 0x18, 0x18, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xE0, 0x70, 0x30, 0x30, 0x30, 0xF0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3F, 0x1F, 0x00, 0x00,

	/* 7 */
	0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* 8 */
	0x00, 0x00, 0xE0, 0xF0, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x83, 0xC7, 0x6C, 0x38, 0x38, 0x38, 0x6C, 0xC7, 0x83, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1F, 0x0F, 0x00, 0x00,

	/* 9 */
	0x00, 0xE0, 0xF0, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0xF0, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1C, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x00, 0x00
};


static void show_time(bool show, unsigned short time)
{
	unsigned char hour = time >> 8;
	unsigned char minute = time & 0xFF;

	if (show) {
		oled_drv_write_block(0, 2, 15, 25, number_16x10[hour / 10]);
		oled_drv_write_block(0, 2, 26, 36, number_16x10[hour % 10]);

		oled_drv_write_block(0, 2, 37, 43, colon_16x6);

		oled_drv_write_block(0, 2, 44, 54, number_16x10[minute / 10]);
		oled_drv_write_block(0, 2, 54, 64, number_16x10[minute / 10]);
	} else {
		oled_drv_fill_block(0, 2, 15, 64, 0);
	}
}

/*
 * 277 => 27.7 du
 */
static void show_temp(bool show, unsigned short temp)
{
	unsigned char ten_digit, single_digit, decimal;

	ten_digit = temp / 100;
	single_digit = (temp % 100) / 10;
	decimal = temp % 10;

	if (show) {
		oled_drv_write_block(2, 5, 10, 23, number_24x13[ten_digit]);
		oled_drv_write_block(2, 5, 23, 36, number_24x13[single_digit]);

		oled_drv_write_block(2, 5, 39, 47, dot_24x8[0]);

		oled_drv_write_block(2, 5, 50, 63, number_24x13[decimal]);

		oled_drv_write_block(2, 5, 65, 85, celsius_24x20[0]);
	} else {
		oled_drv_fill_block(2, 5, 10, 85, 0);
	}


}

static void show_dummy_temp(bool show)
{
	if (show) {
		oled_drv_write_block(2, 5, 10, 23, dummy_celsius_24x13);
		oled_drv_write_block(2, 5, 23, 36, dummy_celsius_24x13);

		oled_drv_write_block(2, 5, 39, 47, dot_24x8[0]);

		oled_drv_write_block(2, 5, 50, 63, dummy_celsius_24x13);

		oled_drv_write_block(2, 5, 65, 85, celsius_24x20[0]);
	} else {
		oled_drv_fill_block(2, 5, 10, 85, 0);
	}
}

static void show_batt(bool show, unsigned char level)
{
	if (show)
		oled_drv_write_block(0, 2, 69, 89, battery_16_20[0]);
	else
		oled_drv_fill_block(0, 2, 69, 89, 0);
}

static void show_bluetooth(bool show)
{
	if (show)
		oled_drv_write_block(0, 2, 0, 10, bluetooth_16_10[0]);
	else
		oled_drv_fill_block(0, 2, 0, 10, 0);
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

		if (od->ble_link == LINK_ON)
			show_bluetooth(TRUE);
		else
			show_bluetooth(FALSE);

		show_batt(TRUE, od->batt_level);

		show_temp(TRUE, od->temp);

		break;

	case OLED_PICTURE2:
		show_time(FALSE, od->time);

		if (od->ble_link == LINK_ON)
			show_bluetooth(TRUE);
		else
			show_bluetooth(FALSE);

		show_batt(FALSE, od->batt_level);

		show_temp(TRUE, od->temp);
		break;

	default:
		print(LOG_WARNING, MODULE "unknown picture to shown!\n");
		break;
	}

	oled_drv_display_on();
}

static void update_first_picture(unsigned char type, unsigned short val)
{
	struct oled_display *od = &display;

	if (od->state != STATE_DISPLAY_ON) {
		print(LOG_WARNING, MODULE "update_first_picture(): err, state is %d\n", od->state);
		return;
	}

	switch (type) {
	case OLED_CONTENT_TIME:
		break;

	case OLED_CONTENT_LINK:
		break;

	case OLED_CONTENT_BATT:
		break;

	case OLED_CONTENT_TEMP:
		show_temp(TRUE, val);

		break;

	case OLED_CONTENT_DUMMY_TEMP:
		show_dummy_temp(TRUE);
		break;

	default:
		break;
	}
}

static void update_seconed_picture(unsigned char type, unsigned short val)
{
}

void oled_update_picture(unsigned char picture, unsigned char type, unsigned short val)
{
	if (picture == OLED_PICTURE1)
		update_first_picture(type, val);
	else if (picture == OLED_PICTURE2)
		update_seconed_picture(type, val);
}

void oled_show_picture(struct display_param *param)
{
	struct oled_display *od = &display;

	switch (param->picture) {
	case OLED_PICTURE1:
		od->batt_level = param->ble_link;
		od->temp = param->temp;
		od->time = param->time;
		od->batt_level = param->batt_level;
		break;

	case OLED_PICTURE2:
		break;

	}

	od->picture = param->picture;
	od->remain_ms = param->remain_ms;
	osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_PREPARE_TIME);
}

void oled_show_next_picture(unsigned short time_ms)
{
	struct oled_display *od = &display;

	od->picture = (od->picture + 1) % OLED_PICTURE_MAX;
	od->remain_ms = time_ms;

	osal_stop_timerEx(od->task_id, TH_DISPLAY_EVT);
	osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_PREPARE_TIME);
}

void oled_display_power_off(void)
{
	struct oled_display *od = &display;

	oled_drv_display_off();

	oled_drv_charge_pump_disable();

	oled_drv_power_off_vcc();

	od->state = STATE_VCC_OFF;
	osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_DELAY_AFTER_VCC_OFF);
}

void oled_display_state_machine(void)
{
	struct oled_display *od = &display;

	switch (od->state) {
	/*
	 * Power On sequence
	 */
	case STATE_POWER_OFF:
		oled_drv_power_on_vdd();
		oled_drv_power_on_vcc();

		od->state = STATE_VDD_ON;
		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_DELAY_AFTER_VDD_ON);
		break;

	case STATE_VDD_ON:
		oled_drv_init_device();

		od->state = STATE_DISPLAY_ON;
		osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, DISPLAY_DELAY_AFTER_VCC_ON);
		break;

	case STATE_DISPLAY_ON:

		if (od->remain_ms == 0) {
			od->event_report(OLED_EVENT_TIME_TO_END, od->picture);

		} else {
			oled_display_draw_picture(od);

			osal_start_timerEx(od->task_id, TH_DISPLAY_EVT, od->remain_ms);
			od->event_report(OLED_EVENT_DISPLAY_ON, od->picture);
			od->remain_ms = 0;
		}

		break;

	case STATE_VCC_OFF:
		oled_drv_power_off_vdd();

		od->state = STATE_POWER_OFF;
		break;
	}
}

void oled_display_init(unsigned char task_id, void (*event_report)(unsigned char event, unsigned short param))
{
	struct oled_display *od = &display;

	print(LOG_INFO, MODULE "oled9639 display init\n");

	od->task_id = task_id;
	od->event_report = event_report;

	oled_drv_init();
}

void oled_display_exit(void)
{
	print(LOG_INFO, MODULE "oled9639 display exit\n");

	oled_drv_exit();
}

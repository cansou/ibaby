/*
 * THER MAIN
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

#include <string.h>

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "OnBoard.h"
#include "hal_adc.h"
#include "hal_led.h"
#include "hal_lcd.h"
#include "hal_key.h"
#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "gatt_profile_uuid.h"
#include "linkdb.h"
#include "peripheral.h"
#include "gapbondmgr.h"
#include "ther_service.h"
#include "devinfoservice.h"
#include "thermometer.h"
#include "OSAL_Clock.h"

#include "config.h"
#include "ther_uart.h"
#include "ther_uart_drv.h"

#include "ther_ble.h"
#include "ther_data.h"

#include "ther_button.h"
#include "ther_buzzer.h"
#include "ther_oled9639_display.h"
#include "ther_mtd.h"
#include "ther_temp.h"
#include "ther_at.h"
#include "ther_misc.h"
#include "ther_batt_service.h"
#include "ther_private_service.h"
#include "ther_port.h"
#include "ther_storage.h"

#define MODULE "[THER   ] "

enum {
	PM_ACTIVE = 0,
	PM_IDLE,
	PM_1,
	PM_2,
	PM_3,
};

struct ther_info ther_info;

#define SEC_TO_MS(sec) ((sec) * 1000)

/**
 * Display
 */
#define DISPLAY_TIME SEC_TO_MS(5)
#define DISPLAY_WELCOME_TIME SEC_TO_MS(2)
#define DISPLAY_GOODBYE_TIME SEC_TO_MS(2)


/*
 * Power on/off
 */
#define SYSTEM_POWER_ON_DELAY 100 /* ms */
#define SYSTEM_POWER_OFF_TIME SEC_TO_MS(3)

/*
 * Temp measurement
 */
#define TEMP_POWER_SETUP_TIME 10 /* ms */
#define TEMP_MEASURE_INTERVAL SEC_TO_MS(3)
#define TEMP_MEASURE_MIN_INTERVAL SEC_TO_MS(1)

#define HIS_TEMP_RESTORE_DELAY 500
#define HIS_TEMP_RESTORE_WAIT_ENABLE SEC_TO_MS(5)
#define HIS_TEMP_UPLOADING_INTERVAL 100

/*
 * Watchdog
 */
#define WATCHDOG_FEED_INTERVAL 500

/*
 * Batt
 */
#define BATT_MEASURE_INTERVAL 120000

static void ther_device_exit_pre(struct ther_info *ti);
static void ther_device_exit_post(struct ther_info *ti);
static void ther_system_power_on(struct ther_info *ti);
static void ther_system_power_off_pre(struct ther_info *ti);
static void ther_system_power_off_post(struct ther_info *ti);

struct ther_info *get_ti(void)
{
	return &ther_info;
}

static void change_measure_timer(struct ther_info *ti, unsigned long new_interval)
{
	if (ti->mode == NORMAL_MODE) {
		osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);

		ti->temp_measure_interval = new_interval;
		osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
	}
}

static void encap_first_picture_param(struct ther_info *ti, struct display_param *param)
{
	unsigned short time;
	UTCTimeStruct utc;

	osal_ConvertUTCTime(&utc, osal_getClock());
	time = ((unsigned short)utc.hour << 8) | utc.minutes;

	param->picture = OLED_PICTURE1;
	param->remain_ms = DISPLAY_TIME;
	param->time = time;
	param->batt_level = 0;
	param->ble_link = LINK_ON;
	param->temp = ti->temp_current;
}

static void ther_handle_button(struct ther_info *ti, struct button_msg *msg)
{
	switch (msg->type) {
	case SHORT_PRESS:
		print(LOG_DBG, MODULE "button pressed\n");

		if (ti->power_mode == PM_ACTIVE) {

			if (ti->display_picture > OLED_PICTURE_NONE) {
				print(LOG_DBG, MODULE "ignore button press when picture is %d\n", ti->display_picture);
				break;
			}

			if (ti->display_picture == OLED_PICTURE_NONE) {
				struct display_param param;

				encap_first_picture_param(ti, &param);
				oled_show_picture(&param);

			} else {
				oled_show_next_picture(DISPLAY_TIME);
			}

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "ignore short press, power down !!!\n");
			ther_system_power_off_post(ti);
		} else {
			print(LOG_DBG, MODULE "unknown power mode\n");
		}

		break;

	case LONG_PRESS:
		print(LOG_DBG, MODULE "button long pressed\n");

		if (ti->power_mode == PM_ACTIVE) {
			ther_system_power_off_pre(ti);

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "power up in long press button\n");
			osal_start_timerEx(ti->task_id, TH_POWER_ON_EVT, SYSTEM_POWER_ON_DELAY);
			ti->power_mode = PM_ACTIVE;
		}

		break;

	default:
		print(LOG_WARNING, MODULE "unknow button press\n");
		break;
	}

	return;
}


static void ther_handle_ts_event(unsigned char event)
{
	struct ther_info *ti = &ther_info;

	switch (event) {

	case THERMOMETER_TEMP_IND_ENABLED:
		print(LOG_INFO, MODULE "enable temp indication\n");

		ti->temp_indication_enable = TRUE;
		ti->indication_interval = 5;

		break;

	case THERMOMETER_TEMP_IND_DISABLED:
		print(LOG_INFO, MODULE "disable temp indication\n");

		ti->temp_indication_enable = FALSE;

		break;

	case THERMOMETER_IMEAS_NOTI_ENABLED:
		print(LOG_INFO, MODULE "enable imeas notification\n");

		ti->temp_notification_enable = TRUE;
		ti->notification_interval = 5;

		break;

	case THERMOMETER_IMEAS_NOTI_DISABLED:
		print(LOG_INFO, MODULE "disable imeas notification\n");
		ti->temp_notification_enable = FALSE;

		break;

	case THERMOMETER_INTERVAL_IND_ENABLED:
		print(LOG_INFO, MODULE "enable interval indication\n");

		break;

	case THERMOMETER_INTERVAL_IND_DISABLED:
		print(LOG_INFO, MODULE "disable interval indication\n");

		break;

	default:
		print(LOG_INFO, MODULE "unknown temp event\n");
		break;
	}

	return;
}

static void ther_handle_ps_event(unsigned char event, unsigned char *data, unsigned char *len)
{
	struct ther_info *ti = &ther_info;
	UTCTimeStruct utc;

	switch (event) {
	case THER_PS_GET_WARNING_ENABLED:
		*data = ti->warning_enabled;
		*len = 1;
		print(LOG_DBG, MODULE "get warning enabled: %d\n", *data);
		break;

	case THER_PS_SET_WARNING_ENABLED:
		ti->warning_enabled = *data;
		print(LOG_DBG, MODULE "set warning enabled: %d\n", *data);
		break;

	case THER_PS_CLEAR_WARNING:
		print(LOG_DBG, MODULE "clear warning: %d\n", *data);
		// TODO
		break;

	case THER_PS_GET_HIGH_WARN_TEMP:
		memcpy(data, &ti->high_temp, sizeof(ti->high_temp));
		*len = sizeof(ti->high_temp);
		print(LOG_DBG, MODULE "get high temp: %d\n", *(unsigned short *)data);
		break;

	case THER_PS_SET_HIGH_WARN_TEMP:
		memcpy(&ti->high_temp, data, sizeof(ti->high_temp));
		print(LOG_DBG, MODULE "set high temp: %d\n", *(unsigned short *)data);
		break;

	case THER_PS_GET_TIME:
		osal_ConvertUTCTime(&utc, osal_getClock());

		memcpy(data, &utc, sizeof(utc));
		*len = sizeof(utc);
		print(LOG_DBG, MODULE "get time: %d-%02d-%02d %02d:%02d:%02d\n",
				utc.year, utc.month, utc.day, utc.hour, utc.minutes, utc.seconds);
		break;

	case THER_PS_SET_TIME:
		memcpy(&utc, data, sizeof(utc));
		osal_setClock(osal_ConvertUTCSecs(&utc));
		print(LOG_DBG, MODULE "set time: %d-%02d-%02d %02d:%02d:%02d\n",
				utc.year, utc.month, utc.day, utc.hour, utc.minutes, utc.seconds);
		break;

	case THER_PS_GET_DEBUG:
		{
			ti->mode = CAL_MODE;
			/*
			 * stop temp measurement
			 */
			osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);
			ther_temp_power_on();
			ti->temp_measure_stage = TEMP_STAGE_SETUP;

			/*
			 * stop batt measurement
			 */
			osal_stop_timerEx(ti->task_id, TH_BATT_EVT);
		}
		*(unsigned short *)data = ther_get_hw_adc(HAL_ADC_CHANNEL_0);
		*len = 2;
		break;

	case THER_PS_SET_DEBUG:
		break;

	default:
		*len = 0;
		break;
	}
}

static void ther_handle_ble_status_change(struct ther_info *ti, struct ble_status_change_msg *msg)
{
	if (msg->type == BLE_DISCONNECT) {
		ti->temp_indication_enable = FALSE;
		ti->temp_notification_enable = FALSE;

		ti->ble_connect = FALSE;
	} else if (msg->type == BLE_CONNECT) {
		ti->ble_connect = TRUE;

		if (!ti->his_temp_uploading) {
			ti->his_temp_uploading = TRUE;
			storage_drain_temp();
			osal_start_timerEx(ti->task_id, TH_HIS_TEMP_RESTORE_EVT, HIS_TEMP_RESTORE_DELAY);
		}
	}

}

static void ther_dispatch_msg(struct ther_info *ti, osal_event_hdr_t *msg)
{
	switch (msg->event) {
	case USER_BUTTON_EVENT:
		ther_handle_button(ti, (struct button_msg *)msg);
		break;

	case GATT_MSG_EVENT:
		ther_handle_gatt_msg((gattMsgEvent_t *)msg);
		break;

	case BLE_STATUS_CHANGE_EVENT:
		ther_handle_ble_status_change(ti, (struct ble_status_change_msg *)msg);
		break;

	default:
		break;
	}
}

static void ther_display_event_report(unsigned char event, unsigned short param)
{
	struct ther_info *ti = &ther_info;

	switch (event) {
	case OLED_EVENT_DISPLAY_ON:
		/* change temp measure to 1 sec */
//		change_measure_timer(ti, TEMP_MEASURE_MIN_INTERVAL);
		ti->display_picture = param;
		break;

	case OLED_EVENT_TIME_TO_END:
//		change_measure_timer(ti, TEMP_MEASURE_INTERVAL);
		if (ti->display_picture == OLED_PICTURE_WELCOME) {
			/* show first picture after welcome */
			struct display_param param;

			encap_first_picture_param(ti, &param);
			oled_show_picture(&param);
		} else {
			ti->display_picture = OLED_PICTURE_NONE;
			oled_display_power_off();
		}

		break;

	}


}

static void ther_device_init(struct ther_info *ti)
{
	/* all gpio init */
	ther_port_init();

	/* uart init */
	ther_uart_init(UART_PORT_0, UART_BAUD_RATE_115200, ther_at_handle);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "-------------------------------------\n");
	print(LOG_INFO, "  Firmware V%d.%d\n",
			FIRMWARE_MAJOR_VERSION, FIREWARM_MINOR_VERSION);
	print(LOG_INFO, "      %s, %s\n",
			__DATE__, __TIME__);

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "  Copyright (c) 2015 59089403@qq.com\r\n");
	print(LOG_INFO, "  All rights reserved.\r\n");
	print(LOG_INFO, "-------------------------------------\r\n");

	/* button init */
	ther_button_init(ti->task_id);

	/* buzzer init */
	ther_buzzer_init(ti->task_id);

	/* oled display init */
	oled_display_init(ti->task_id, ther_display_event_report);

	/* mtd */
	ther_mtd_init();

	/* storage */
	ther_storage_init();

	/* temp init */
	ther_temp_init();

	/* ble init */
	ther_ble_init(ti->task_id, ther_handle_ts_event, ther_handle_ps_event);

//	HCI_EXT_ClkDivOnHaltCmd( HCI_EXT_ENABLE_CLK_DIVIDE_ON_HALT );

	// Enable stack to toggle bypass control on TPS62730 (DC/DC converter)
//	HCI_EXT_MapPmIoPortCmd( HCI_EXT_PM_IO_PORT_NONE, HCI_EXT_PM_IO_PORT_PIN0 );

  //the TI interface to set TXPOWER is not satisfying, so I have to
  //set register TXPOWER myself.
//  HCI_EXT_SetTxPowerCmd(LL_EXT_TX_POWER_0_DBM);
}

static void ther_device_exit_pre(struct ther_info *ti)
{
	/* ble init */
	ther_ble_exit();

	ther_storage_exit();
}

static void ther_device_exit_post(struct ther_info *ti)
{
	/* oled display exit */
	oled_display_exit();
}

static void ther_system_power_on(struct ther_info *ti)
{
	struct display_param param;

	ther_device_init(ti);

	/*
	 * play music
	 */
	ther_buzzer_play_music(BUZZER_MUSIC_SYS_POWER_ON);

	/*
	 * show welcome picture
	 */
	param.picture = OLED_PICTURE_WELCOME;
	param.remain_ms = DISPLAY_WELCOME_TIME;
	oled_show_picture(&param);

	/*
	 * start temp measurement
	 */
	ti->temp_measure_interval = TEMP_MEASURE_INTERVAL;
	ti->temp_measure_stage = TEMP_STAGE_SETUP;
	osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);

	/*
	 * batt measure
	 */
	osal_start_timerEx( ti->task_id, TH_BATT_EVT, BATT_MEASURE_INTERVAL);


	/* test */
//	osal_start_timerEx(ti->task_id, TH_TEST_EVT, 5000);
}

static void ther_system_power_off_pre(struct ther_info *ti)
{
	struct display_param param;

	/* test */
	osal_stop_timerEx(ti->task_id, TH_TEST_EVT);

	/*
	 * batt measure
	 */
	osal_stop_timerEx(ti->task_id, TH_BATT_EVT);

	/*
	 * stop temp measurement
	 */
	osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);

	/*
	 * play power off music
	 */
	ther_buzzer_stop_music();
	ther_buzzer_play_music(BUZZER_MUSIC_SYS_POWER_OFF);

	/*
	 * oled says goodbye
	 */
	param.picture = OLED_PICTURE_GOODBYE;
	param.remain_ms = DISPLAY_GOODBYE_TIME;
	oled_show_picture(&param);

	osal_start_timerEx(ti->task_id, TH_POWER_OFF_EVT, SYSTEM_POWER_OFF_TIME);

	ther_device_exit_pre(ti);
}

static void ther_system_power_off_post(struct ther_info *ti)
{
	ther_device_exit_post(ti);

	/*
	 * do not stop wd timer here,
	 * so the wd timer will be auto running after power on
	 */
/*	osal_stop_timerEx(ti->task_id, TH_WATCHDOG_EVT);*/

	ti->power_mode = PM_3;
	/* go to PM3 */
    SLEEPCMD |= BV(0) | BV(1);
    PCON |=BV(0);
}

/*********************************************************************
 * @fn      Thermometer_ProcessEvent
 *
 * @brief   Thermometer Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 Thermometer_ProcessEvent(uint8 task_id, uint16 events)
{
	struct ther_info *ti = &ther_info;

	/* message handle */
	if ( events & SYS_EVENT_MSG ) {
		uint8 *msg;

		if ( (msg = osal_msg_receive(ti->task_id)) != NULL ) {
			ther_dispatch_msg(ti, (osal_event_hdr_t *)msg);

			osal_msg_deallocate( msg );
		}

		return (events ^ SYS_EVENT_MSG);
	}

	if (events & TH_POWER_ON_EVT) {
		ther_system_power_on(ti);

		return (events ^ TH_POWER_ON_EVT);
	}

	if (events & TH_POWER_OFF_EVT) {
		ther_system_power_off_post(ti);

		return (events ^ TH_POWER_OFF_EVT);
	}

	/* batt measure */
	if (events & TH_BATT_EVT) {
		if (ti->mode == NORMAL_MODE) {
			Batt_MeasLevel();
			ti->batt_percentage = ther_batt_get_percentage(FALSE);
			print(LOG_DBG, MODULE "batt %d%%\n", ti->batt_percentage);

			osal_start_timerEx( ti->task_id, TH_BATT_EVT, BATT_MEASURE_INTERVAL);
		}

		return (events ^ TH_BATT_EVT);
	}

	/* temp measure event */
	if (events & TH_TEMP_MEASURE_EVT) {

		if (ti->mode != NORMAL_MODE) {
			return (events ^ TH_TEMP_MEASURE_EVT);
		}

		switch (ti->temp_measure_stage) {
		case TEMP_STAGE_SETUP:
			ther_temp_power_on();

			osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);
			ti->temp_measure_stage = TEMP_STAGE_MEASURE;
			break;

		case TEMP_STAGE_MEASURE:

			ti->temp_last_saved = ti->temp_current;
			ti->temp_current = ther_get_temp();
			ther_temp_power_off();

			if (ti->ble_connect) {
				if (ti->temp_notification_enable) {
					ther_send_temp_notify(ti->temp_current);
				} else if (ti->temp_indication_enable) {
//					ther_send_temp_indicate(ti->task_id, ti->temp_current);
				}
			} else {
				ther_save_temp_to_local(ti->temp_current);
			}

			if (ti->display_picture == OLED_PICTURE1 &&
				ti->temp_current != ti->temp_last_saved) {
				oled_update_picture(OLED_PICTURE1, OLED_CONTENT_TEMP, ti->temp_current);
			}

			osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
			ti->temp_measure_stage = TEMP_STAGE_SETUP;
			break;

		default:
			break;
		}

		return (events ^ TH_TEMP_MEASURE_EVT);
	}

	if (events & TH_HIS_TEMP_RESTORE_EVT) {
		if (!ti->his_temp_bundle) {
			if (ti->temp_indication_enable) {
				storage_restore_temp((uint8 **)&ti->his_temp_bundle, &ti->his_temp_len);
				if (ti->his_temp_bundle) {
					ti->his_temp_offset = 0;
					osal_start_timerEx(ti->task_id, TH_HIS_TEMP_RESTORE_EVT, HIS_TEMP_UPLOADING_INTERVAL);
				} else {
					print(LOG_DBG, MODULE "his temp restore: no more his temp, exit\n");
					ti->his_temp_uploading = FALSE;
				}

			} else if (!ti->ble_connect) {
				print(LOG_DBG, MODULE "his temp restore: ble disconnect, exit\n");
				ti->his_temp_uploading = FALSE;

			} else {
//				print(LOG_DBG, MODULE "his temp restore: wait for indication enable\n");
				osal_start_timerEx(ti->task_id, TH_HIS_TEMP_RESTORE_EVT, HIS_TEMP_RESTORE_WAIT_ENABLE);
			}

		} else {
			if (ti->his_temp_offset < ti->his_temp_len) {
				uint8 *data = ti->his_temp_bundle + ti->his_temp_offset;

				ther_send_history_temp(ti->task_id, data, sizeof(struct temp_data));

				ti->his_temp_offset += sizeof(struct temp_data);
			} else {
				ti->his_temp_bundle = NULL;
				ti->his_temp_offset = 0;
				ti->his_temp_len = 0;
				print(LOG_DBG, MODULE "his temp restore: a bundle uploading completed\n");
			}
			osal_start_timerEx(ti->task_id, TH_HIS_TEMP_RESTORE_EVT, HIS_TEMP_UPLOADING_INTERVAL);
		}

		return (events ^ TH_HIS_TEMP_RESTORE_EVT);
	}

	/* Display event */
	if (events & TH_DISPLAY_EVT) {

		oled_display_state_machine();

		return (events ^ TH_DISPLAY_EVT);
	}

	/* buzzer event */
	if (events & TH_BUZZER_EVT) {
		ther_buzzer_check_music();

		return (events ^ TH_BUZZER_EVT);
	}

	/* button event */
	if (events & TH_BUTTON_EVT) {
		ther_measure_button_time();

		return (events ^ TH_BUTTON_EVT);
	}

	if (events & TH_WATCHDOG_EVT) {
		feed_watchdog();
		osal_start_timerEx(ti->task_id, TH_WATCHDOG_EVT, WATCHDOG_FEED_INTERVAL);

		return (events ^ TH_WATCHDOG_EVT);
	}


	if (events & TH_TEST_EVT) {
//		oled_picture_inverse();

		print(LOG_DBG, MODULE "live\n");

//		oled_show_temp(TRUE, ti->current_temp);

//		ther_spi_w25x_test(0,1,32);

//		print(LOG_DBG, "ADC0 %d\n", ther_get_adc(0));
//		print(LOG_DBG, "ADC1 %d\n", ther_get_adc(1));

		osal_start_timerEx(ti->task_id, TH_TEST_EVT, 1000);

		return (events ^ TH_TEST_EVT);
	}

	return 0;
}


/*********************************************************************
 * @fn      Thermometer_Init
 *
 * @brief   Initialization function for the Thermometer App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void Thermometer_Init(uint8 task_id)
{
	struct ther_info *ti = &ther_info;

	ti->task_id = task_id;

	ti->power_mode = PM_ACTIVE;

//	start_watchdog_timer();
	osal_start_timerEx( ti->task_id, TH_WATCHDOG_EVT, WATCHDOG_FEED_INTERVAL);

	osal_start_timerEx(ti->task_id, TH_POWER_ON_EVT, SYSTEM_POWER_ON_DELAY);
}

/*
 * Just for compiling
 */
void HalLedEnterSleep(void) {}

/*
 * Just for compiling
 */
void HalLedExitSleep(void) {}

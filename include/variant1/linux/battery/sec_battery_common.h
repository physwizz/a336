/*
 * sec_battery_common.h
 * Samsung Mobile Charging Common Header
 *
 * Copyright (C) 2020 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SEC_BATTERY_COMMON_H
#define __SEC_BATTERY_COMMON_H __FILE__

#include <linux/power_supply.h>

enum power_supply_ext_property {
	POWER_SUPPLY_EXT_PROP_MIN = 1000,
	POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C = POWER_SUPPLY_EXT_PROP_MIN,
	POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_CMD,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_VAL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID,
	POWER_SUPPLY_EXT_PROP_WIRELESS_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SWITCH,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_VIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_IIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONNECTED,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_POWER,
	POWER_SUPPLY_EXT_PROP_WIRELESS_MAX_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_STATUS,	
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_DATA,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_SIZE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_TYPE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_MIN_DUTY,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_INITIAL_WC_CHECK,
	POWER_SUPPLY_EXT_PROP_WIRELESS_PARAM_INFO,
	POWER_SUPPLY_EXT_PROP_WIRELESS_CHECK_FW_VER,
	POWER_SUPPLY_EXT_PROP_AICL_CURRENT,
	POWER_SUPPLY_EXT_PROP_CHECK_MULTI_CHARGE,
	POWER_SUPPLY_EXT_PROP_CHIP_ID,
	POWER_SUPPLY_EXT_PROP_ERROR_CAUSE,
	POWER_SUPPLY_EXT_PROP_SYSOVLO,
	POWER_SUPPLY_EXT_PROP_VBAT_OVP,
	POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING,
	POWER_SUPPLY_EXT_PROP_USB_CONFIGURE,
	POWER_SUPPLY_EXT_PROP_WDT_STATUS,
	POWER_SUPPLY_EXT_PROP_WATER_DETECT,
	POWER_SUPPLY_EXT_PROP_SURGE,
	POWER_SUPPLY_EXT_PROP_HV_DISABLE,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_RESET,
	POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION,
	POWER_SUPPLY_EXT_PROP_ANDIG_IVR_SWITCH,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY,
	POWER_SUPPLY_EXT_PROP_DISABLE_FACTORY_MODE,
	POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_NOTIFY,
	POWER_SUPPLY_EXT_PROP_CHARGE_POWER,
	POWER_SUPPLY_EXT_PROP_MEASURE_SYS,
	POWER_SUPPLY_EXT_PROP_MEASURE_INPUT,
	POWER_SUPPLY_EXT_PROP_WC_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHGINSEL,
	POWER_SUPPLY_EXT_PROP_JIG_GPIO,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_HICCUP,
	POWER_SUPPLY_EXT_PROP_MONITOR_WORK,
	POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST,
	POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TIMER_ON,
	POWER_SUPPLY_EXT_PROP_CALL_EVENT,
	POWER_SUPPLY_EXT_PROP_DEFAULT_CURRENT,
#if defined(CONFIG_DUAL_BATTERY)
	POWER_SUPPLY_EXT_PROP_CHGIN_OK,
	POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE,
	POWER_SUPPLY_EXT_PROP_RECHG_ON,
	POWER_SUPPLY_EXT_PROP_EOC_ON,
	POWER_SUPPLY_EXT_PROP_DISCHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_CHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_TRICKLECHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_RECHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_CURRENT,
	POWER_SUPPLY_EXT_PROP_POWERMETER_ENABLE,
	POWER_SUPPLY_EXT_PROP_TSD_ENABLE,
	POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET,
#endif
	POWER_SUPPLY_EXT_PROP_CURRENT_EVENT,
	POWER_SUPPLY_EXT_PROP_CURRENT_EVENT_CLEAR,
	POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL,
	POWER_SUPPLY_EXT_PROP_MAX_DUTY_EVENT,
	POWER_SUPPLY_EXT_PROP_VCHGIN_CHANGE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_AVG_CURR,
	POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE,
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC,
	POWER_SUPPLY_EXT_PROP_DIRECT_DONE,
	POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL,
	POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_FLOAT_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL,
	POWER_SUPPLY_EXT_PROP_DIRECT_POWER_TYPE,
	POWER_SUPPLY_EXT_PROP_DIRECT_HV_PDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_FAILED,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_READY,
	POWER_SUPPLY_EXT_PROP_DIRECT_DETACHED,
	POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
	POWER_SUPPLY_EXT_PROP_DIRECT_HARD_RESET,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_DISABLE,
	POWER_SUPPLY_EXT_PROP_DIRECT_HAS_APDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT,
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS,
	POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE,
	POWER_SUPPLY_EXT_PROP_DIRECT_CLEAR_ERR,
	POWER_SUPPLY_EXT_PROP_DIRECT_SEND_UVDM,
	POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA,
	POWER_SUPPLY_EXT_PROP_SRCCAP,
	POWER_SUPPLY_EXT_PROP_WD_QBATTOFF,
	POWER_SUPPLY_EXT_PROP_WPC_DET_STATUS,
	POWER_SUPPLY_EXT_PROP_CHARGE_BOOST,
	POWER_SUPPLY_EXT_PROP_CHARGE_MODE,
	POWER_SUPPLY_EXT_PROP_FLED_BOOST_ON,
	POWER_SUPPLY_EXT_PROP_FLED_BOOST_OFF,
	POWER_SUPPLY_EXT_PROP_BYPASS_MODE_DISABLE,
	POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_EXT_PROP_CURRENT_FULL,
	POWER_SUPPLY_EXT_PROP_POWER_DESIGN,
	POWER_SUPPLY_EXT_PROP_FILTER_CFG,
	POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_WPC_EN,
	POWER_SUPPLY_EXT_PROP_WPC_EN_MST,
	POWER_SUPPLY_EXT_PROP_HEALTH,
	POWER_SUPPLY_EXT_PROP_BOOT_COMPLETE,
	POWER_SUPPLY_EXT_PROP_SLEEP_MODE,
	POWER_SUPPLY_EXT_PROP_MFC_FW_UPDATE,
	POWER_SUPPLY_EXT_PROP_THERMAL_ZONE,
	POWER_SUPPLY_EXT_PROP_DC_INITIALIZE,
	POWER_SUPPLY_EXT_PROP_BATTERY_ID,
#if defined(CONFIG_DUAL_BATTERY_CELL_SENSING)
	POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK,
#endif
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL,
	POWER_SUPPLY_EXT_PROP_INPUT_CURRENT_LIMIT_WRL,
	POWER_SUPPLY_EXT_PROP_CONSTANT_CHARGE_CURRENT_WRL,
	POWER_SUPPLY_EXT_PROP_SUB_TEMP,
	POWER_SUPPLY_EXT_PROP_MIX_LIMIT,
	POWER_SUPPLY_EXT_PROP_MAX,
};

enum sec_battery_usb_conf {
	USB_CURRENT_NONE = 0,
	USB_CURRENT_SUSPENDED = 1,
	USB_CURRENT_UNCONFIGURED = 100,
	USB_CURRENT_HIGH_SPEED = 475,
	USB_CURRENT_SUPER_SPEED = 850,
};

enum sec_battery_cable {
	SEC_BATTERY_CABLE_UNKNOWN = 0,
	SEC_BATTERY_CABLE_NONE,			/* 1 */
	SEC_BATTERY_CABLE_PREPARE_TA,		/* 2 */
	SEC_BATTERY_CABLE_TA,			/* 3 */
	SEC_BATTERY_CABLE_USB,			/* 4 */
	SEC_BATTERY_CABLE_USB_CDP,		/* 5 */
	SEC_BATTERY_CABLE_9V_TA,		/* 6 */
	SEC_BATTERY_CABLE_9V_ERR,		/* 7 */
	SEC_BATTERY_CABLE_9V_UNKNOWN,		/* 8 */
	SEC_BATTERY_CABLE_12V_TA,		/* 9 */
	SEC_BATTERY_CABLE_WIRELESS,		/* 10 */
	SEC_BATTERY_CABLE_HV_WIRELESS,		/* 11 */
	SEC_BATTERY_CABLE_PMA_WIRELESS,		/* 12 */
	SEC_BATTERY_CABLE_WIRELESS_PACK,	/* 13 */
	SEC_BATTERY_CABLE_WIRELESS_HV_PACK,	/* 14 */
	SEC_BATTERY_CABLE_WIRELESS_STAND,	/* 15 */
	SEC_BATTERY_CABLE_WIRELESS_HV_STAND,	/* 16 */
	SEC_BATTERY_CABLE_QC20,			/* 17 */
	SEC_BATTERY_CABLE_QC30,			/* 18 */
	SEC_BATTERY_CABLE_PDIC,			/* 19 */
	SEC_BATTERY_CABLE_UARTOFF,		/* 20 */
	SEC_BATTERY_CABLE_OTG,			/* 21 */
	SEC_BATTERY_CABLE_LAN_HUB,		/* 22 */
	SEC_BATTERY_CABLE_POWER_SHARING,	/* 23 */
	SEC_BATTERY_CABLE_HMT_CONNECTED,	/* 24 */
	SEC_BATTERY_CABLE_HMT_CHARGE,		/* 25 */
	SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT,	/* 26 */
	SEC_BATTERY_CABLE_WIRELESS_VEHICLE,		/* 27 */
	SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE,	/* 28 */
	SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV,	/* 29 */
	SEC_BATTERY_CABLE_TIMEOUT,	        /* 30 */
	SEC_BATTERY_CABLE_SMART_OTG,            /* 31 */
	SEC_BATTERY_CABLE_SMART_NOTG,           /* 32 */
	SEC_BATTERY_CABLE_WIRELESS_TX,		/* 33 */
	SEC_BATTERY_CABLE_HV_WIRELESS_20,	/* 34 */
	SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT,	/* 35 */
	SEC_BATTERY_CABLE_WIRELESS_FAKE, /* 36 */
	SEC_BATTERY_CABLE_PREPARE_WIRELESS_20,	/* 37 */
	SEC_BATTERY_CABLE_PDIC_APDO,		/* 38 */
	SEC_BATTERY_CABLE_MAX,			/* 39 */
};

static inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return (struct power_supply *)NULL;
	else
		return power_supply_get_by_name(name);
}

#define psy_do_property(name, function, property, value) \
({	\
	struct power_supply *psy;	\
	int ret = 0;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		pr_err("%s: Fail to "#function" psy (%s)\n",	\
			__func__, (name));	\
		value.intval = 0;	\
		ret = -ENOENT;	\
	} else {	\
		if (psy->desc->function##_property != NULL) { \
			ret = psy->desc->function##_property(psy, \
				(enum power_supply_property) (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to %s "#function" (%d=>%d)\n", \
						__func__, name, (property), ret);	\
				value.intval = 0;	\
			}	\
		} else {	\
			ret = -ENOSYS;	\
		}	\
		power_supply_put(psy);		\
	}					\
	ret;	\
})

#endif /* __SEC_BATTERY_COMMON_H */

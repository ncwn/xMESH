#!/bin/env python3
import json
import os
import shutil
import sys
import argparse

config_template = {
    "CALIB": {
        "CALIB_ZE07_CO_ADC_MULTI": "0.205",
        "CALIB_ZE07_CO_ADC_BIAS": "-73.8",
        "CALIB_ZE07_CO_BIAS": "-0.5"
    },
    "COMMS": {
        "HAZEMON_ENABLE":	"false",
	    "HAZEMON_SYNC_INTERVAL":	"false",
	    "HAZEMON_IP":	"203.159.6.98",
	    "HAZEMON_PORT":	"60002",
	    "HAZEMON_PROJECT_ID":	"1",
	    "LORARELAY_ENABLE":	"false",
	    "LORARELAY_GPS_CYCLE":	"5",
	    "MQTT_DATA_ENABLE":	"false",
	    "MQTT_CONFIGURATION_ENABLE":	"false",
	    "MQTT_URI":	"test.mosquitto.org",
	    "MQTT_PORT":	"1883",
	    "MQTT_USERNAME":	"",
	    "MQTT_PASSWORD":	""
    },
    "GENERAL": {
        "DEVICE_DATA_MODE":	"CAN5_DATA_MODE_CYCLE",
	    "DATA_CYCLE_SEC":	"120",
	    "WIFI_SNTP_SERVER":	"pool.ntp.org",
	    "OTA_UPDATE_URL":	"https://lora.hazemon.in.th/can5/firmware/",
	    "INIT":	"true",
	    "DEVICE_NAME":	"can5-device",
	    "DEVICE_ORGANIZATION":	"interlab"
    },
    "GPS": {
        "LAST_G_LAT":	"14.0775217",
	    "LAST_G_LNG":	"100.61316",
	    "LAST_G_ALT":	"3.4"
    },
    "LOG": {
        "LOG_TO_SD":	"false",
	    "LOG_TO_NETSOCK":	"false",
	    "LOG_TO_NETSOCK_IP":	"192.168.2.216",
	    "LOG_TO_NETSOCK_PORT":	"9745"
    },
    "NETIF": {
        "WIFI_AP_ENABLE":	"true",
	    "WIFI_AP_PASS":	"interlab",
	    "WIFI_STA_ENABLE":	"false",
    	"WIFI_STA_SSID":	"",
	    "WIFI_STA_PASS":	"",
	    "LWAN_ENABLE":	"false",
	    "LWAN_OTAA":	"false",
	    "LWAN_APPEUI":	"99:88:77:66:99:88:77:66",
	    "LWAN_DADDR":	"00:00:00:00",
	    "LWAN_RX_1_DELAY":	"5000",
	    "LWAN_RX_2_DELAY":	"6000",
	    "LWAN_ADAPTIVE_DATA_RATE":	"0",
	    "LWAN_DATA_RATE":	"5",
	    "LWAN_TRANSMIT_POWER":	"0",
	    "LWAN_USER_DATALEN":	"0",
	    "CELL_ENABLE":	"false",
	    "CELL_APN":	"internet",
	    "CELL_ENABLE_GPS":	"false"
    },
    "SENSORS": {
        "ADC_0": "",
        "ADC_1": "",
        "ADC_2": "",
        "ADC_3": "",
        "I2C_0": "",
        "I2C_1": "",
        "I2C_2": "",
        "I2C_3": "",
        "I2C_4": "CAN5_SENSORDRIV_TYPE_BME280",
        "I2C_5": "",
        "I2C_6": "",
        "I2C_7": "",
        "UART_0": "",
        "UART_1": "CAN5_SENSORDRIV_TYPE_PM",
        "UART_2": "",
        "UART_3": "CAN5_SENSORDRIV_TYPE_CO2",
        "UART_4": "",
        "UART_5": "",
        "UART_6": "CAN5_SENSORDRIV_TYPE_GPS",
        "UART_7": "CAN5_SENSORDRIV_TYPE_CO"
    }
}


def write_config(config, config_template, cfile, out_path):
    for directory in ['CONFIG', 'DATA/P_DATA', 'LOG']:
        os.makedirs(os.path.join(out_path, directory), exist_ok=True)

    for key in config_template.keys():
        ofile = os.path.join(out_path, 'CONFIG', key)
        template = config_template[key]
        for common_key in set(config.keys()).intersection(set(template.keys())):
            template[common_key] = config[common_key]
        with open(ofile, 'w') as f:
            json.dump(template, f, indent=2)

    shutil.copy(cfile, os.path.join(out_path, 'CONFIG/CRONTAB'))

    open(os.path.join(out_path, 'DATA/P_DATA/1'), 'w').close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Provision a FAT32 formatted SD card.")
    parser.add_argument("-pfile", required=True, help="Provision input file.")
    parser.add_argument("-cfile", required=True, help="Cron input file.")
    parser.add_argument("outpath", help="SD card path.")

    args = parser.parse_args(sys.argv[1:])

    with open(args.pfile, 'r') as f:
        write_config(json.load(f), config_template, args.cfile, args.outpath)

    print("Successfully provisioned!")

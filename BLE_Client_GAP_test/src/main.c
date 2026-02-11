/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>

//BLE
#include <zephyr/bluetooth/conn.h>					//need for bonnection creation
#include <zephyr/bluetooth/bluetooth.h>				//need for bt enable, scan and scan stop
													//includes gap.h, addr.h and hci_types already
#include <zephyr/bluetooth/gap.h>					//need for bonnection creation
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/hci.h>
//#include <zephyr/bluetooth/uuid.h>
//#include <zephyr/bluetooth/gatt.h>
//#include <zephyr/sys/byteorder.h>

//other drivers
#include <zephyr/logging/log.h>

//logger module
LOG_MODULE_REGISTER(BLE_Client_GAP_test,LOG_LEVEL_DBG);

//the server name we are searching for
static char device_name_to_find[5] = {'W', 'B', '5', 'M', 'M'};;

static void start_scan(void);

static struct bt_conn *default_conn;

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

/*	if (default_conn) {
		return;
	}*/

	// we only are looking for certain type of devices (no beacons)
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}

	// we only want to check devices that are close to us
	if (rssi < -50) {
		return;
	}

	//we add a char pointer to the data section
	uint8_t* char_ptr = ad->data;

	//device name is in data stored as a string. We turn the string back toa char array
	uint8_t device_name_len = (ad->len) - 2 - 6;

	//we step the pointer twice to remove the string header
	char_ptr++;
	char_ptr++;

	for(uint8_t i = 0; i<device_name_len; i++)						//we extarct the device name as a char array
	{
		if(device_name_to_find[i] != *char_ptr){

			return;

		} else {

			char_ptr++;

		};

	}

	//we extract the address of the device we have found
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	//we print out the name of the device, the address and the
	//Note: the ad->data of the WB5MM is a string with a TAB to start and \n to close it. Thus the weird printout to match visually the device name
	LOG_INF("Device found: \n\r %s \r        %s \n\r        (RSSI %d)\n", ad->data, addr_str, rssi);

	//we stop scanning
	if (bt_le_scan_stop()) {
		return;
	}

	//after we have found our device and stopped scanning, we sleep a little
	//we sleep here before we connect so as to avoid crashing the BLE stack
	k_msleep(10000);

	//we construct our connection using default connection parameters (BT_CONN_LE_CREATE_CONN and BT_LE_CONN_PARAM_DEFAULT) on the device address and the connection handle
	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);

	//in case of failure
	if (err) {
		LOG_ERR("Create conn to %s failed (%d)\n", addr_str, err);
		start_scan();
	}
}

//scan start function
static void start_scan(void)
{
	int err;

	// we start passive scanning
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);

	if (err) {
		LOG_ERR("Scanning failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Scanning started for device %s\n", device_name_to_find);
}

//connected callback
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	//we extract the connected device's address
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s %u %s\n", addr, err, bt_hci_err_to_str(err));

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	LOG_INF("Connected: %s\n", addr);

	//we disconnect from the device with an error code
	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

}

//dc callback
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Disconnected: %s, reason 0x%02x %s \n", addr, reason, bt_hci_err_to_str(reason));
	printk("\n");

	//unreference connection handle
	bt_conn_unref(default_conn);
	default_conn = NULL;

	//restart scanning
	start_scan();

}

//connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;

	//enable BLE
	err = bt_enable(NULL);

	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized\n");

	//start first scane
	start_scan();
	return 0;
}

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
#include <zephyr/bluetooth/gatt.h>
//#include <zephyr/sys/byteorder.h>

//other drivers
#include <zephyr/logging/log.h>

//logger module
LOG_MODULE_REGISTER(BLE_Client_GATT_test,LOG_LEVEL_DBG);

//the server name we are searching for
static char device_name_to_find[5] = {'W', 'B', '5', 'M', 'M'};

static void start_scan(void);

static struct bt_conn *default_conn;

//-------------MTU------------//
static struct bt_gatt_exchange_params mtu_exchange_params;

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_exchange_params *params)
{
	LOG_INF("%s: MTU exchange %s (%u)\n", __func__,
	       err == 0U ? "successful" : "failed",
	       bt_gatt_get_mtu(conn));
}

static int mtu_exchange(struct bt_conn *conn)
{
	int err;

	LOG_INF("%s: Current MTU = %u\n", __func__, bt_gatt_get_mtu(conn));

	mtu_exchange_params.func = mtu_exchange_cb;

	LOG_INF("%s: Exchange MTU...\n", __func__);
	err = bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
	if (err) {
		LOG_INF("%s: MTU  failed (err %d)", __func__, err);
	}

	return err;
}

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	LOG_INF("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
}

static struct bt_gatt_cb ble_gatt_callbacks = {
	.att_mtu_updated = mtu_updated											//we only have a GATT callback on the mtu change
};

//-------------Scan-------------//
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
//	k_msleep(10000);

	//we construct our connection using default connection parameters (BT_CONN_LE_CREATE_CONN and BT_LE_CONN_PARAM_DEFAULT) on the device address and the connection handle
	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);

	//in case of failure
	if (err) {
		LOG_ERR("Create conn to %s failed (%d)\n", addr_str, err);
		start_scan();
	} else {
		bt_conn_unref(default_conn);
	}
}

//scan start function
static void start_scan(void)
{
	int err;

	// we start passive scanning
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

	if (err) {
		LOG_ERR("Scanning failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Scanning started for device %s\n", device_name_to_find);
}

//-------------Connection callback------------//
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

	default_conn = bt_conn_ref(conn);

	(void)mtu_exchange(conn);

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

	//unreference connection handle
	bt_conn_unref(default_conn);
	default_conn = NULL;

	//restart scanning
	start_scan();

}

//connection callbacks
BT_CONN_CB_DEFINE(ble_conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

//write with response callback
static void write_cmd_cb (struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params){

	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

}


//read callback
static uint8_t read_cmd_cb (struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length){

	uint8_t * readout = data;									//we reference the data pointer as a uint8_t since that is the data package size we will have on the WB5MM

	if (err) {
		LOG_ERR("Readout failed (err %d)\n", err);
	} else {
		LOG_INF("Readout from WB5MM %x\n", *readout);
	}

	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	return err;

}

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

	bt_gatt_cb_register(&ble_gatt_callbacks);			//register GATT callbacks

	//start first scan
	start_scan();

	//infinite loop
	while(1){

		struct bt_conn *conn_superloop = NULL;

		if (default_conn) {											//check if we are connected, then 		
			
			conn_superloop = bt_conn_ref(default_conn);				//returns the reference count of the connection
		}

		if (conn_superloop) {										//if the refernce is not 0, we are connected
			
			//Note: uuid values are adjustead to match the custom server running on the WB5MM (see "STM32_BLE_Custom_Server_Read" repo on Github)

//			#ifdef writing_to_wb5mm_oled
			//here we are enabling a notification with a write (no response) command
			//the text will be published on the OLED screen of the WB5MM
			static uint8_t text_to_send[] = {'h','e','l','l','o',' ','n','r','f','5','2','!'};

			static uint16_t wb5mm_write_uuid_offset = 0x0001;

			//below are wb5mm internal uuid offset values
			static uint16_t wb5mm_write_uuid_char_offset = 0x0001;
			static uint16_t wb5mm_uuid_offset = 0x000c;

			struct bt_gatt_write_params ble_write_parameters  = {

				.func = write_cmd_cb,
				.handle = (wb5mm_write_uuid_offset + wb5mm_write_uuid_char_offset + wb5mm_uuid_offset),
				.offset = 0,
				.data = text_to_send,
				.length = sizeof(text_to_send),

			};

			err = bt_gatt_write(conn_superloop, &ble_write_parameters);

			if (err) {
				LOG_ERR("%s: Write cmd failed (%d).\n", __func__, err);
			} else {
				LOG_INF("Command sent...\n");
			}
//			#endif

			#ifdef notification_to_wb5mm
			//here we are enabling a notification witn a write (no response) command
			//we will have a counter on the OLED screen

			//enable notification
			static uint8_t notification_bool_switch[1] = {0x1};

			//disable notification
//			static uint8_t notification_bool_switch[1] = {0x0};

			static uint16_t wb5mm_notification_uuid_offset = 0x0004;

			//below are wb5mm internal uuid offset values
			static uint16_t wb5mm_notification_descriptor_uuid_offset = 0x0002;
			static uint16_t wb5mm_uuid_offset = 0x000c;
						
			err = bt_gatt_write_without_response_cb(conn_superloop, (wb5mm_notification_uuid_offset + wb5mm_notification_descriptor_uuid_offset + wb5mm_uuid_offset), notification_bool_switch, sizeof(notification_bool_switch), false, write_cmd_cb, NULL);

			if (err) {
				LOG_ERR("%s: Write cmd failed (%d).\n", __func__, err);
			} else {
				LOG_INF("Command sent...\n");
			}		
			#endif

			#ifdef reading_from_wb5mm
			//here we are reading out the read characteristic from the WB5MM as a hex value
			//the temperature will be published on the OLED screen of the WB5MM as decimal

			static uint16_t wb5mm_read_uuid_char_offset = 0x0009;
			static uint16_t wb5mm_uuid_offset = 0x000c;

			struct bt_gatt_read_params ble_read_parameters  = {

				.func = read_cmd_cb,
				.handle_count = 1,														//selects which way we want to read out. 0 is by uuid, 1 is single, 2 or mroe is multiple
				.single.handle = (wb5mm_uuid_offset + wb5mm_read_uuid_char_offset),		//this will be 0x0015
				.single.offset = 0,

			};

			err = bt_gatt_read(conn_superloop, &ble_read_parameters);

			if (err) {
				LOG_ERR("%s: Read cmd failed (%d).\n", __func__, err);
			} else {
				LOG_INF("Command sent...\n");
			}
			#endif			
			
			bt_conn_unref(conn_superloop);
			k_msleep(10000);
			k_yield();
		} else {
			k_sleep(K_SECONDS(1));
		}

	}

	return 0;
}

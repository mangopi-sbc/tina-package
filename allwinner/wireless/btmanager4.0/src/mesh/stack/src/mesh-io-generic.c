/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2018-2019  Intel Corporation. All rights reserved.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <ell/ell.h>

#include "monitor/bt.h"
#include "mesh/xr829-patch.h"
#ifdef CONFIG_XR829_BT
#include "src/bluez-5.54_shared/hci.h"
#else
#include "src/shared/hci.h"
#endif
#include "src/bluez-5.54_shared/io.h"
#include "lib/bluetooth.h"
#include "lib/mgmt.h"
#include "lib/hci.h"
#include <fcntl.h>
#include "mesh/mesh-defs.h"
#include "mesh/mesh-mgmt.h"
#include "mesh/mesh-io.h"
#include "mesh/mesh-io-api.h"
#include "mesh/mesh-io-generic.h"

#ifdef CONFIG_XR829_BT
#include "mesh/dbus.h"
#include "mesh/node.h"
#include "mesh/mesh.h"
#endif

#ifdef CONFIG_XR829_BT
#define LL_ADV_DELAY_MS    10
struct l_timeout *exception_timeout = NULL;
static uint32_t g_mesh_adv_seqn = 0;
static uint32_t g_mesh_adv_cnt = 0;
typedef struct {
	uint32_t seqn;
    uint32_t cnt;
} __attribute__ ((packed)) le_set_adv_stat_cmd_cp;
#define LE_SET_ADV_STAT_CMD_CP_SIZE 8
#define OGF_LE_VENDOR		        0x3F
#define OCF_SET_ADV_STAT_CMD        0x003C
#define HCI_CMD_SET_ADV_STAT_VENDOR 0xFC3C
struct bt_hci_evt_adv_stat_evt_t {
    uint32_t seqn;
    uint32_t cnt;
} __attribute__ ((packed));

typedef uint8_t app_addr_t[6];

typedef struct{
    uint8_t  device_uuid[16];
    uint16_t oob_info;
    uint8_t  uri_hash[4];
    uint8_t  mac[6];
}app_mesh_unprov_beacon_t;

typedef enum {
    APP_ADDRESS_TYPE_PUBLIC, // public address
    APP_ADDRESS_TYPE_RANDOM, // random address
} app_addr_type_t;

typedef enum {
    ADV_DATA,           // advertising data
    SCAN_RSP_DATA,      // response data from active scanning
} app_gap_adv_data_type_t;

typedef struct {
    app_addr_t peer_addr;
    app_addr_type_t addr_type;
    app_gap_adv_data_type_t adv_type;
    int8_t rssi;
    uint8_t data_len;
    uint8_t data[31];
} app_gap_adv_report_t;

#endif

struct bluez554_bt_hci {
	int ref_count;
	struct io *io;
	bool is_stream;
	bool writer_active;
	uint8_t num_cmds;
	unsigned int next_cmd_id;
	unsigned int next_evt_id;
	struct queue *cmd_queue;
	struct queue *rsp_queue;
	struct queue *evt_list;
};

struct mesh_io_private {
	struct bluez554_bt_hci *hci;
	void *user_data;
	mesh_io_ready_func_t ready_callback;
	struct l_timeout *tx_timeout;
	struct l_queue *rx_regs;
	struct l_queue *tx_pkts;
	struct tx_pkt *tx;
	uint16_t index;
	uint16_t interval;
	bool sending;
	bool active;
};

struct pvt_rx_reg {
	mesh_io_recv_func_t cb;
	void *user_data;
	uint8_t len;
	uint8_t filter[0];
};

struct process_data {
	struct mesh_io_private		*pvt;
	const uint8_t			*data;
	uint8_t				len;
	struct mesh_io_recv_info	info;
};

struct tx_pkt {
	struct mesh_io_send_info	info;
	bool				delete;
	uint8_t				len;
	uint8_t				pkt[30];
};

struct tx_pattern {
	const uint8_t			*data;
	uint8_t				len;
};

static uint32_t get_instant(void)
{
	struct timeval tm;
	uint32_t instant;

	gettimeofday(&tm, NULL);
	instant = tm.tv_sec * 1000;
	instant += tm.tv_usec / 1000;

	return instant;
}

static uint32_t instant_remaining_ms(uint32_t instant)
{
	instant -= get_instant();
	return instant;
}

static void process_rx_callbacks(void *v_reg, void *v_rx)
{
	struct pvt_rx_reg *rx_reg = v_reg;
	struct process_data *rx = v_rx;

	if (!memcmp(rx->data, rx_reg->filter, rx_reg->len))
		rx_reg->cb(rx_reg->user_data, &rx->info, rx->data, rx->len);
}

static void process_rx(struct mesh_io_private *pvt, int8_t rssi,
					uint32_t instant, const uint8_t *addr,
					const uint8_t *data, uint8_t len)
{
	struct process_data rx = {
		.pvt = pvt,
		.data = data,
		.len = len,
		.info.instant = instant,
		.info.addr = addr,
		.info.chan = 7,
		.info.rssi = rssi,
	};

	l_queue_foreach(pvt->rx_regs, process_rx_callbacks, &rx);
}

#ifdef CONFIG_XR829_BT
app_mesh_unprov_beacon_t param_unprov;
static void mi_mesh_adv_report(struct mesh_node *node, const void *buf, uint8_t size)
{
	const struct bt_hci_evt_le_adv_report *evts = (void *)buf;
	app_gap_adv_report_t param;

    int8_t *rssis;
    uint8_t evt_len = 0, num_reports = 0, i;

    num_reports = evts->num_reports;

    for (i = 0; i < num_reports; i++) {
		rssis = (int8_t *) (evts->data + evts->data_len);
		memcpy(param.peer_addr, evts->addr, 6);
		param.addr_type = (evts->addr_type == 0x00) ?
				APP_ADDRESS_TYPE_PUBLIC:APP_ADDRESS_TYPE_RANDOM;
		if (evts->event_type == 0x04)
			param.adv_type = SCAN_RSP_DATA;
		else
			param.adv_type = ADV_DATA;
		param.rssi = *rssis;
		//param.data_len = evts->data_len;
		//memcpy(param.data, evts->data, evts->data_len);
		if (evts->data_len <= 31) {
			memcpy(param.data, evts->data, evts->data_len);
			param.data_len = evts->data_len;
		} else {
			memcpy(param.data, evts->data, 31);
			param.data_len = 31;
		}

		struct l_dbus_message *msg;
		struct l_dbus *dbus = dbus_get_bus();
		struct l_dbus_message_builder *builder;
		//struct mesh_adapter *adapter = mesh_adapter_get_instance();

		char *method_name = "AdvPacket";
		msg = l_dbus_message_new_method_call(dbus, node_get_owner(node)/*node->owner*/,
									node_get_app_path(node)/*node->app_path*/,
									/*MESH_PROVISIONER_INTERFACE,*/
									MESH_APPLICATION_INTERFACE,
									method_name);

		builder = l_dbus_message_builder_new(msg);
		dbus_append_byte_array(builder, (const uint8_t *)&param.peer_addr, 6);
		l_dbus_message_builder_append_basic(builder, 'y', &param.addr_type);
		l_dbus_message_builder_append_basic(builder, 'y', &param.adv_type);
		l_dbus_message_builder_append_basic(builder, 'n', &param.rssi);
		l_dbus_message_builder_append_basic(builder, 'y', &param.data_len);
		dbus_append_byte_array(builder, (const uint8_t *)&param.data, param.data_len);
		l_dbus_message_builder_finalize(builder);
		l_dbus_message_builder_destroy(builder);
		l_dbus_send(dbus, msg);

		evt_len = sizeof(*evts) + evts->data_len + 1;
		if (size > evt_len) {
			buf += evt_len - 1;
			size -= evt_len - 1;
			evts = (void *)buf;
		} else {
			break;
		}
    }

}

static void mi_process_rx_callbacks(void *v_rx, const void *buf, uint8_t size)
{
	struct pvt_rx_reg *rx_reg = v_rx;
	struct mesh_node *node;

	if((rx_reg->cb != NULL) && (rx_reg->user_data != NULL)) {
		node = (struct mesh_node *)rx_reg->user_data;
		if(node == xr_get_scan_node())
			mi_mesh_adv_report(node, buf, size);
	}
}

static void mi_mesh_unprov_scan_report(struct mesh_node *node, const uint8_t *adv, uint8_t len)
{
	app_mesh_unprov_beacon_t param;
	//l_info("mi_mesh_unprov_scan_report len is %d\n", adv[0] + 1);
	print_packet("unprov scan adv:", adv, len);
	memcpy(&param.device_uuid, adv + 3, 16);
	memcpy(&param.oob_info, adv + 3 + 16, 2);
	if (len > 21)
		memcpy(&param.uri_hash, adv + 3 + 16 + 2, len -21);

	struct l_dbus_message *msg;
	struct l_dbus *dbus = dbus_get_bus();
	struct l_dbus_message_builder *builder;

	char *method_name = "UnprovDevice";/*mi_mesh_event_to_dbus_method(MIBLE_MESH_EVENT_UNPROV_DEVICE);*/
	msg = l_dbus_message_new_method_call(dbus, node_get_owner(node)/*node->owner*/,
					node_get_app_path(node)/*node->app_path*/,
					/*MESH_PROVISIONER_INTERFACE,*/
					MESH_APPLICATION_INTERFACE,
					method_name);

	builder = l_dbus_message_builder_new(msg);

	dbus_append_byte_array(builder, (const uint8_t *)&param.device_uuid, 16);
	l_dbus_message_builder_append_basic(builder, 'n', &param.oob_info);
	dbus_append_byte_array(builder, (const uint8_t *)&param.mac, 6);
	dbus_append_byte_array(builder, (const uint8_t *)&param.uri_hash, len-21);
	l_dbus_message_builder_finalize(builder);
	l_dbus_message_builder_destroy(builder);
	l_dbus_send(dbus, msg);
}

static void mi_process_unprov_rx_callbacks(void *v_rx, const uint8_t *buf, uint8_t size)
{
	struct pvt_rx_reg *rx_reg = v_rx;
	struct mesh_node *node;

	if((rx_reg->cb != NULL) && (rx_reg->user_data != NULL)) {
		node = (struct mesh_node *)rx_reg->user_data;
		if(node == xr_get_scan_node()) {
			mi_mesh_unprov_scan_report(node, buf, size);
		}
	}
}
#endif

static void event_adv_report(struct mesh_io *io, const void *buf, uint8_t size)
{
	const struct bt_hci_evt_le_adv_report *evt = buf;
	const uint8_t *adv;
	const uint8_t *addr;
	uint32_t instant;
	uint8_t adv_len;
	uint16_t len = 0;
	int8_t rssi;

	if (evt->event_type != 0x03)
		return;

	instant = get_instant();
	adv = evt->data;
	adv_len = evt->data_len;
	addr = evt->addr;

	/* rssi is just beyond last byte of data */
	rssi = (int8_t) adv[adv_len];

	while (len < adv_len - 1) {
		uint8_t field_len = adv[0];

		/* Check for the end of advertising data */
		if (field_len == 0)
			break;

		len += field_len + 1;

		/* Do not continue data parsing if got incorrect length */
		if (len > adv_len)
			break;
#ifdef CONFIG_XR829_BT
		if (xr_get_scan_node() != NULL && adv[1] == 0x2b && adv[2] == 0x00) {
			struct l_queue_entry *entry;
			if (unlikely(!io->pvt->rx_regs))
				return;
			memcpy(&param_unprov.mac, evt->addr, 6);
			for (entry = (struct l_queue_entry *)l_queue_get_entries(io->pvt->rx_regs); entry; entry = entry->next) {
				if(entry != NULL)
					mi_process_unprov_rx_callbacks(entry->data, adv, len);
			}

		}
#endif
		/* TODO: Create an Instant to use */
		process_rx(io->pvt, rssi, instant, addr, adv + 1, adv[0]);

		adv += field_len + 1;
	}
#ifdef CONFIG_XR829_BT
	/*adapter for xiaomi mesh api*/
	if (xr_get_scan_node() != NULL) {
		struct l_queue_entry *entry;
		if (unlikely(!io->pvt->rx_regs))
			return;

		for (entry = (struct l_queue_entry *)l_queue_get_entries(io->pvt->rx_regs); entry; entry = entry->next) {
			if((entry != NULL)&&(adv[1] == 0x2B))
				mi_process_rx_callbacks(entry->data, buf, size);
		}

	}
	/*end*/
#endif
}

static void event_callback(const void *buf, uint8_t size, void *user_data)
{
	uint8_t event = l_get_u8(buf);
	struct mesh_io *io = user_data;

	switch (event) {
	case BT_HCI_EVT_LE_ADV_REPORT:
		event_adv_report(io, buf + 1, size - 1);
		break;

	default:
		l_info("Other Meta Evt - %d", event);
	}
}

#ifdef CONFIG_XR829_BT
static void complete_event_callback(const void *buf, uint8_t size, void *user_data)
{
    struct bt_hci_evt_adv_stat_evt_t *evt = (void *)buf;
    if(size == sizeof(struct bt_hci_evt_adv_stat_evt_t))
    {
        l_info("tx_to seqn %x cnt %x\n",evt->seqn,evt->cnt);
    }
    else
    {
        l_info("tx_to cmpl evt len =%d  sturct size = %d\n",size,sizeof(struct bt_hci_evt_adv_stat_evt_t));
    }
}
#endif
static void local_commands_callback(const void *data, uint8_t size,
							void *user_data)
{
	const struct bt_hci_rsp_read_local_commands *rsp = data;

	if (rsp->status)
		l_error("Failed to read local commands");
}

static void local_features_callback(const void *data, uint8_t size,
							void *user_data)
{
	const struct bt_hci_rsp_read_local_features *rsp = data;

	if (rsp->status)
		l_error("Failed to read local features");
}

static void hci_generic_callback(const void *data, uint8_t size,
								void *user_data)
{
	uint8_t status = l_get_u8(data);

	if (status)
		l_error("Failed to initialize HCI");
}

static void configure_hci(struct mesh_io_private *io)
{
	struct bt_hci_cmd_le_set_scan_parameters cmd;
#ifndef CONFIG_XR829_BT
	struct bt_hci_cmd_set_event_mask cmd_sem;
#endif
	struct bt_hci_cmd_le_set_event_mask cmd_slem;

	/* Set scan parameters */
	cmd.type = 0x00; /* Passive Scanning. No scanning PDUs shall be sent */
	cmd.interval = 0x0030; /* Scan Interval = N * 0.625ms */
	cmd.window = 0x0030; /* Scan Window = N * 0.625ms */
	cmd.own_addr_type = 0x00; /* Public Device Address */
	/* Accept all advertising packets except directed advertising packets
	 * not addressed to this device (default).
	 */
	cmd.filter_policy = 0x00;

	/* Set event mask
	 *
	 * Mask: 0x2000800002008890
	 *   Disconnection Complete
	 *   Encryption Change
	 *   Read Remote Version Information Complete
	 *   Hardware Error
	 *   Data Buffer Overflow
	 *   Encryption Key Refresh Complete
	 *   LE Meta
	 */
#ifndef CONFIG_XR829_BT
	cmd_sem.mask[0] = 0x90;
	cmd_sem.mask[1] = 0x88;
	cmd_sem.mask[2] = 0x00;
	cmd_sem.mask[3] = 0x02;
	cmd_sem.mask[4] = 0x00;
	cmd_sem.mask[5] = 0x80;
	cmd_sem.mask[6] = 0x00;
	cmd_sem.mask[7] = 0x20;
#endif
	/* Set LE event mask
	 *
	 * Mask: 0x000000000000087f
	 *   LE Connection Complete
	 *   LE Advertising Report
	 *   LE Connection Update Complete
	 *   LE Read Remote Used Features Complete
	 *   LE Long Term Key Request
	 *   LE Remote Connection Parameter Request
	 *   LE Data Length Change
	 *   LE PHY Update Complete
	 */
	cmd_slem.mask[0] = 0x7f;
	cmd_slem.mask[1] = 0x08;
	cmd_slem.mask[2] = 0x00;
	cmd_slem.mask[3] = 0x00;
	cmd_slem.mask[4] = 0x00;
	cmd_slem.mask[5] = 0x00;
	cmd_slem.mask[6] = 0x00;
	cmd_slem.mask[7] = 0x00;

	/* TODO: Move to suitable place. Set suitable masks */
	/* Reset Command */
	XR_CFG_SEND_HCI_CMD_PATCH(io->hci, BT_HCI_CMD_RESET, NULL, 0, hci_generic_callback,
								NULL, NULL);

	/* Read local supported commands */
	bluez554_bt_hci_send(io->hci, BT_HCI_CMD_READ_LOCAL_COMMANDS, NULL, 0,
					local_commands_callback, NULL, NULL);

	/* Read local supported features */
	bluez554_bt_hci_send(io->hci, BT_HCI_CMD_READ_LOCAL_FEATURES, NULL, 0,
					local_features_callback, NULL, NULL);

	/* Set event mask */
	XR_CFG_SEND_HCI_CMD_PATCH(io->hci, BT_HCI_CMD_SET_EVENT_MASK, &cmd_sem,
			sizeof(cmd_sem), hci_generic_callback, NULL, NULL);

	/* Set LE event mask */
	bluez554_bt_hci_send(io->hci, BT_HCI_CMD_LE_SET_EVENT_MASK, &cmd_slem,
			sizeof(cmd_slem), hci_generic_callback, NULL, NULL);

	/* Scan Params */
	bluez554_bt_hci_send(io->hci, BT_HCI_CMD_LE_SET_SCAN_PARAMETERS, &cmd,
				sizeof(cmd), hci_generic_callback, NULL, NULL);
}

static void hci_init(void *user_data)
{
	struct mesh_io *io = user_data;
	bool result = true;
	XR_HCI_INIT_PATCH(io->pvt->hci,io->pvt->index);
	if (!io->pvt->hci) {
		l_error("Failed to start mesh io (hci %u): %s", io->pvt->index,
							strerror(errno));
		result = false;
	}

	if (result) {
		configure_hci(io->pvt);

		bluez554_bt_hci_register(io->pvt->hci, BT_HCI_EVT_LE_META_EVENT,
						event_callback, io, NULL);

		l_debug("Started mesh on hci %u", io->pvt->index);
	}

	if (io->pvt->ready_callback)
		io->pvt->ready_callback(io->pvt->user_data, result);
}

static void read_info(int index, void *user_data)
{
	struct mesh_io *io = user_data;

	if (io->pvt->index != MGMT_INDEX_NONE &&
					index != io->pvt->index) {
		l_debug("Ignore index %d", index);
		return;
	}

	io->pvt->index = index;
	hci_init(io);
}

static bool dev_init(struct mesh_io *io, void *opts,
				mesh_io_ready_func_t cb, void *user_data)
{
	if (!io || io->pvt)
		return false;

	io->pvt = l_new(struct mesh_io_private, 1);
	io->pvt->index = *(int *)opts;

	io->pvt->rx_regs = l_queue_new();
	io->pvt->tx_pkts = l_queue_new();

	io->pvt->ready_callback = cb;
	io->pvt->user_data = user_data;

	if (io->pvt->index == MGMT_INDEX_NONE)
		return mesh_mgmt_list(read_info, io);

	l_idle_oneshot(hci_init, io, NULL);

	return true;
}

static bool dev_destroy(struct mesh_io *io)
{
	struct mesh_io_private *pvt = io->pvt;

	if (!pvt)
		return true;

	bluez554_bt_hci_unref(pvt->hci);
	l_timeout_remove(pvt->tx_timeout);
	l_queue_destroy(pvt->rx_regs, l_free);
	l_queue_destroy(pvt->tx_pkts, l_free);
    pvt->tx_timeout = NULL;
	l_free(pvt);
	io->pvt = NULL;

	return true;
}

static bool dev_caps(struct mesh_io *io, struct mesh_io_caps *caps)
{
	struct mesh_io_private *pvt = io->pvt;

	if (!pvt || !caps)
		return false;

	caps->max_num_filters = 255;
	caps->window_accuracy = 50;

	return true;
}

static void send_cancel_done(const void *buf, uint8_t size,
							void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct bt_hci_cmd_le_set_random_address cmd;

	if (!pvt)
		return;

	pvt->sending = false;

	/* At end of any burst of ADVs, change random address */
	l_getrandom(cmd.addr, 6);
	cmd.addr[5] |= 0xc0;
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_RANDOM_ADDRESS,
				&cmd, sizeof(cmd), NULL, NULL, NULL);
}

static void send_cancel(struct mesh_io_private *pvt)
{
	struct bt_hci_cmd_le_set_adv_enable cmd;

	if (!pvt)
		return;

	if (!pvt->sending) {
		send_cancel_done(NULL, 0, pvt);
		return;
	}

	cmd.enable = 0x00;	/* Disable advertising */
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_ADV_ENABLE,
				&cmd, sizeof(cmd),
				send_cancel_done, pvt, NULL);
}

static int hci_send_cmd(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
	uint8_t type = HCI_COMMAND_PKT;
	hci_command_hdr hc;
	struct iovec iv[3];
	int ivn;
    int flags;
    flags = fcntl(dd,F_GETFL,0);
    flags &= ~O_NONBLOCK;
    fcntl(dd,F_SETFL,flags);
	hc.opcode = htobs(cmd_opcode_pack(ogf, ocf));
	hc.plen= plen;

	iv[0].iov_base = &type;
	iv[0].iov_len  = 1;
	iv[1].iov_base = &hc;
	iv[1].iov_len  = HCI_COMMAND_HDR_SIZE;
	ivn = 2;

	if (plen) {
		iv[2].iov_base = param;
		iv[2].iov_len  = plen;
		ivn = 3;
	}

	while (writev(dd, iv, ivn) < 0) {
		if (errno == EAGAIN || errno == EINTR)
			continue;
		return -1;
	}
    flags |= O_NONBLOCK;
    fcntl(dd,F_SETFL,flags);
	return 0;
}

static void send_adv_enable(int dd, uint8_t enable)
{
    le_set_advertise_enable_cp adv_cp;
	//le_set_advertise_enable_cp cp;
	memset(&adv_cp, 0, sizeof(adv_cp));
	adv_cp.enable = enable;

    hci_send_cmd(dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISE_ENABLE, LE_SET_ADVERTISE_ENABLE_CP_SIZE, &adv_cp);

	return ;
}

static void send_adv_stat_cmd(struct mesh_io_private *pvt)
{
    le_set_adv_stat_cmd_cp adv_stat_cp;

    if(g_mesh_adv_cnt <= 1)
    {
        return ;
    }
	memset(&adv_stat_cp, 0, sizeof(adv_stat_cp));
	adv_stat_cp.seqn = g_mesh_adv_seqn;
    adv_stat_cp.cnt = g_mesh_adv_cnt;
	bluez554_bt_hci_send(pvt->hci, HCI_CMD_SET_ADV_STAT_VENDOR,
			&adv_stat_cp, sizeof(adv_stat_cp), complete_event_callback, NULL, NULL);
	return ;
}

static void send_adv_params(int dd, uint16_t	min_interval, uint16_t	max_interval, uint8_t advtype, uint8_t own_bdaddr_type,uint8_t chan_map,uint8_t filter_policy)
{
	le_set_advertising_parameters_cp adv_params_cp;

	memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = min_interval;
    adv_params_cp.max_interval = max_interval;
    adv_params_cp.advtype = advtype;
    adv_params_cp.own_bdaddr_type = own_bdaddr_type;
    adv_params_cp.chan_map = chan_map;
    hci_send_cmd(dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_PARAMETERS, LE_SET_ADVERTISING_PARAMETERS_CP_SIZE, &adv_params_cp);
	return ;
}

static void send_adv_data(int dd, uint8_t length, uint8_t *data)
{
	le_set_advertising_data_cp adv_data_cp;
	memset(&adv_data_cp, 0, sizeof(adv_data_cp));
	adv_data_cp.length = length + 1;
    adv_data_cp.data[0] = length;
	memcpy(&adv_data_cp.data[1], data, length);
    hci_send_cmd(dd, OGF_LE_CTL, OCF_LE_SET_ADVERTISING_DATA, LE_SET_ADVERTISING_DATA_CP_SIZE, &adv_data_cp);
	return ;
}

static void le_send_pkt(struct mesh_io_private *pvt, struct tx_pkt *tx,
							uint16_t interval)
{
	//struct bt_hci_cmd_le_set_adv_enable cmd;
    int dd = 0;

	if (pvt->tx && pvt->tx->delete)
	{
		xr_error(TRC_URG,"%s,last pvt->tx fail\n",__func__);
		l_free(pvt->tx);
	}
    else
    {
        if(pvt->tx)
        {
            xr_error(TRC_URG,"%s,tx_to pvt->tx not null\n",__func__);
        }
		/*
        else
        {
            xr_warn(TRC_URG,"%s,tx_to\n",__func__);
        }
		*/
    }
	pvt->tx = tx;
	pvt->interval = interval;
    pvt->sending = true;
    dd = bluez554_io_get_fd(pvt->hci->io);
    send_adv_stat_cmd(pvt);
    send_adv_enable(dd,0);
    interval = (pvt->interval * 16) / 10;
    send_adv_params(dd,interval,interval,0x03,0x00,0x07,0x03);
    send_adv_data(dd,tx->len,tx->pkt);
    send_adv_enable(dd,1);

     if (tx->delete)
         l_free(tx);
     pvt->tx = NULL;

}
#ifdef CONFIG_XR829_BT
#else

static void set_send_adv_enable(const void *buf, uint8_t size,
							void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct bt_hci_cmd_le_set_adv_enable cmd;

	if (!pvt)
		return;

	pvt->sending = true;
	cmd.enable = 0x01;	/* Enable advertising */
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_ADV_ENABLE,
				&cmd, sizeof(cmd), NULL, NULL, NULL);
}


static void set_send_adv_data(const void *buf, uint8_t size,
							void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct tx_pkt *tx;
	struct bt_hci_cmd_le_set_adv_data cmd;

	if (!pvt || !pvt->tx)
		return;

	tx = pvt->tx;
	if (tx->len >= sizeof(cmd.data))
		goto done;

	memset(&cmd, 0, sizeof(cmd));

	cmd.len = tx->len + 1;
	cmd.data[0] = tx->len;
	memcpy(cmd.data + 1, tx->pkt, tx->len);

	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_ADV_DATA,
					&cmd, sizeof(cmd),
					set_send_adv_enable, pvt, NULL);
done:
	if (tx->delete)
		l_free(tx);

	pvt->tx = NULL;
}

static void set_send_adv_params(const void *buf, uint8_t size,
							void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct bt_hci_cmd_le_set_adv_parameters cmd;
	uint16_t hci_interval;

	if (!pvt)
		return;

	hci_interval = (pvt->interval * 16) / 10;
	cmd.min_interval = L_CPU_TO_LE16(hci_interval);
	cmd.max_interval = L_CPU_TO_LE16(hci_interval);
	cmd.type = 0x03; /* ADV_NONCONN_IND */
	cmd.own_addr_type = 0x00; /* ADDR_TYPE_PUBLIC */
	cmd.direct_addr_type = 0x00;
	memset(cmd.direct_addr, 0, 6);
	cmd.channel_map = 0x07;
	cmd.filter_policy = 0x03;

	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_ADV_PARAMETERS,
				&cmd, sizeof(cmd),
				set_send_adv_data, pvt, NULL);
}

static void send_pkt(struct mesh_io_private *pvt, struct tx_pkt *tx,
							uint16_t interval)
{
	struct bt_hci_cmd_le_set_adv_enable cmd;

	if (pvt->tx && pvt->tx->delete)
	{
		l_free(pvt->tx);
	}
    else
    {
        if(pvt->tx)
        {
            xr_error(TRC_URG,"%s,tx_to pvt->tx not null\n",__func__);
        }
    }
	pvt->tx = tx;
	pvt->interval = interval;

	if (!pvt->sending) {
		set_send_adv_params(NULL, 0, pvt);
		return;
	}

	cmd.enable = 0x00;	/* Disable advertising */
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_ADV_ENABLE,
				&cmd, sizeof(cmd),
				set_send_adv_params, pvt, NULL);
}
#endif
#ifdef CONFIG_XR829_BT
extern int32_t beacon_num;
#endif
void print_time(const char *label)
{
	struct timeval pkt_time;

	gettimeofday(&pkt_time, NULL);

    xr_warn(TRC_MESH_IO,"%s:%05d.%03d\n",label,(uint32_t) pkt_time.tv_sec % 100000,(uint32_t) pkt_time.tv_usec/1000);

}

struct timeval mible_adv_time;
uint16_t mible_adv_interval;
int8_t adv_is_on = 0;

static void tx_to(struct l_timeout *timeout, void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct tx_pkt *tx = NULL;
    uint16_t send_ms;
	uint16_t ms;
	uint8_t count;
    uint8_t repeat = 0;
#ifdef CONFIG_XR829_BT
    bool ret = false;
    if(exception_timeout)
    {
        l_timeout_remove(exception_timeout);
        exception_timeout = NULL;
    }
    if(pvt)
    {
        tx = l_queue_pop_head(pvt->tx_pkts);
		adv_is_on = 1;
        if (!tx) {
			adv_is_on = 0;
            l_timeout_remove(timeout);
            pvt->tx_timeout = NULL;
            send_cancel(pvt);
            ret = true;
        }
    }
    else
    {
        ret = true;
    }

    if(ret == true)
    {
	adv_is_on = 0;
        //print_time("tx_to tx pkt 0 adv_is_on is 0");
        return;
    }
    else
    {
        //print_time("tx_to tx pkt 1");
    }

#else
	if (!pvt)
		return;

	tx = l_queue_pop_head(pvt->tx_pkts);
	if (!tx) {
		l_timeout_remove(timeout);
		pvt->tx_timeout = NULL;
		send_cancel(pvt);
		return;
	}
#endif

	if (!tx) {
		return;
	}
	if (tx->info.type == MESH_IO_TIMING_TYPE_GENERAL) {
#ifdef CONFIG_XR829_BT
        count = tx->info.u.gen.cnt;
        repeat = count;
        ms = tx->info.u.gen.interval;
        send_ms = ms;
        if(count != MESH_IO_TX_COUNT_UNLIMITED)
        {
            send_ms += LL_ADV_DELAY_MS;
            send_ms *= count;
            count = 1;
            tx->info.u.gen.cnt = 1;
        }
#else
        ms = tx->info.u.gen.interval;
		count = tx->info.u.gen.cnt;
		if (count != MESH_IO_TX_COUNT_UNLIMITED)
			tx->info.u.gen.cnt--;
#endif
	} else {
		ms = 25;
		count = 1;
        send_ms = ms;
	}

	tx->delete = !!(count == 1);
#ifdef CONFIG_XR829_BT
	if (beacon_num > 1 ) {
		beacon_num--;
		l_debug("beacon_count = %d\n", beacon_num - 1);
	}

	if(beacon_num == 1){
		beacon_num = 0;
		l_debug("tx_timeout beacon_num = 0\n");
		l_timeout_remove(timeout);
		pvt->tx_timeout = NULL;
		send_cancel(pvt);
		return;
	}
#endif
#ifdef CONFIG_XR829_BT
    if(repeat > 1)
    {
        g_mesh_adv_cnt = repeat;
        g_mesh_adv_seqn++;
        //l_info("g_mesh_adv_cnt %x g_mesh_adv_seqn %x\n",g_mesh_adv_cnt,g_mesh_adv_seqn);
    }
    le_send_pkt(pvt, tx, ms);
#else
	send_pkt(pvt, tx, ms);
#endif
	if (count == 1) {
		/* Recalculate wakeup if we are responding to POLL */
		tx = l_queue_peek_head(pvt->tx_pkts);
		if (tx && tx->info.type == MESH_IO_TIMING_TYPE_POLL_RSP) {
			ms = instant_remaining_ms(tx->info.u.poll_rsp.instant +
						tx->info.u.poll_rsp.delay);
            send_ms = ms;
		}
	} else
		l_queue_push_tail(pvt->tx_pkts, tx);

	if (timeout) {
		pvt->tx_timeout = timeout;
		l_timeout_modify_ms(timeout, send_ms);
        //l_info("%s,tx_to modify ms %d\n",__func__,send_ms);
	}
    else
    {
		pvt->tx_timeout = l_timeout_create_ms(send_ms, tx_to, pvt, NULL);
        //l_info("%s,tx_to create ms %d\n",__func__,send_ms);
    }
}

static void tx_worker(void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct tx_pkt *tx;
	uint32_t delay;
	//l_info("%s\n",__func__);
	tx = l_queue_peek_head(pvt->tx_pkts);
	if (!tx)
		return;

	switch (tx->info.type) {
	case MESH_IO_TIMING_TYPE_GENERAL:
		if (tx->info.u.gen.min_delay == tx->info.u.gen.max_delay)
			delay = tx->info.u.gen.min_delay;
		else {
			l_getrandom(&delay, sizeof(delay));
			delay %= tx->info.u.gen.max_delay -
						tx->info.u.gen.min_delay;
			delay += tx->info.u.gen.min_delay;
		}
		break;

	case MESH_IO_TIMING_TYPE_POLL:
		if (tx->info.u.poll.min_delay == tx->info.u.poll.max_delay)
			delay = tx->info.u.poll.min_delay;
		else {
			l_getrandom(&delay, sizeof(delay));
			delay %= tx->info.u.poll.max_delay -
						tx->info.u.poll.min_delay;
			delay += tx->info.u.poll.min_delay;
		}
		break;

	case MESH_IO_TIMING_TYPE_POLL_RSP:
		/* Delay until Instant + Delay */
		delay = instant_remaining_ms(tx->info.u.poll_rsp.instant +
						tx->info.u.poll_rsp.delay);
		if (delay > 255)
			delay = 0;
		break;

	default:
		return;
	}

	if (!delay)
		tx_to(pvt->tx_timeout, pvt);
	else if (pvt->tx_timeout)
		l_timeout_modify_ms(pvt->tx_timeout, delay);
	else
		pvt->tx_timeout = l_timeout_create_ms(delay, tx_to, pvt, NULL);

}
#ifdef CONFIG_XR829_BT
void exception_func(struct l_timeout *timeout,void *user_data)
{
    struct mesh_io_private *pvt = user_data;
    xr_error(TRC_URG,"%s 1s timer happen!\n",__func__);
    l_timeout_remove(pvt->tx_timeout);
	pvt->tx_timeout = NULL;
    if(l_queue_peek_head(pvt->tx_pkts))
    {
        l_idle_oneshot(tx_worker, pvt, NULL);
    }
	l_timeout_remove(timeout);
    l_info("%s,remove tx_to timer\n",__func__);
	exception_timeout = NULL;
}
#endif

static bool send_tx(struct mesh_io *io, struct mesh_io_send_info *info,
					const uint8_t *data, uint16_t len)
{
	struct mesh_io_private *pvt = io->pvt;
	struct tx_pkt *tx;
	bool sending = false;

	if (!info || !data || !len || len > sizeof(tx->pkt))
		return false;

	tx = l_new(struct tx_pkt, 1);
	if (!tx)
		return false;

	memcpy(&tx->info, info, sizeof(tx->info));
	memcpy(&tx->pkt, data, len);
	tx->len = len;

	if (info->type == MESH_IO_TIMING_TYPE_POLL_RSP)
		l_queue_push_head(pvt->tx_pkts, tx);
	else {
		if (pvt->tx)
			sending = true;
		else
			sending = !l_queue_isempty(pvt->tx_pkts);
		l_queue_push_tail(pvt->tx_pkts, tx);
	}
#ifdef CONFIG_XR829_BT
    if (!sending) {
        if(pvt->tx_timeout == NULL)
        {
            l_idle_oneshot(tx_worker, pvt, NULL);
        }
        else
        {
            xr_warn(TRC_URG,"%s,pvt->tx_timeout not free ,start 1s timer to free it!",__func__);
            if(exception_timeout == NULL)
            {
                exception_timeout = l_timeout_create(1,&exception_func,pvt,NULL);
            }
        }
    }
#else
	if (!sending) {
		l_timeout_remove(pvt->tx_timeout);
		pvt->tx_timeout = NULL;
		l_idle_oneshot(tx_worker, pvt, NULL);
	}
#endif
	return true;
}

static bool find_by_ad_type(const void *a, const void *b)
{
	const struct tx_pkt *tx = a;
	uint8_t ad_type = L_PTR_TO_UINT(b);

	return !ad_type || ad_type == tx->pkt[0];
}

static bool find_by_pattern(const void *a, const void *b)
{
	const struct tx_pkt *tx = a;
	const struct tx_pattern *pattern = b;

	if (tx->len < pattern->len)
		return false;

	return (!memcmp(tx->pkt, pattern->data, pattern->len));
}

static bool tx_cancel(struct mesh_io *io, const uint8_t *data, uint8_t len)
{
	struct mesh_io_private *pvt = io->pvt;
	struct tx_pkt *tx;

	if (!data)
		return false;

	if (len == 1) {
		do {
			tx = l_queue_remove_if(pvt->tx_pkts, find_by_ad_type,
							L_UINT_TO_PTR(data[0]));
			l_free(tx);

			if (tx == pvt->tx)
				pvt->tx = NULL;

		} while (tx);
	} else {
		struct tx_pattern pattern = {
			.data = data,
			.len = len
		};

		do {
			tx = l_queue_remove_if(pvt->tx_pkts, find_by_pattern,
								&pattern);
			l_free(tx);

			if (tx == pvt->tx)
				pvt->tx = NULL;

		} while (tx);
	}

	if (l_queue_isempty(pvt->tx_pkts)) {
		send_cancel(pvt);
		l_timeout_remove(pvt->tx_timeout);
		pvt->tx_timeout = NULL;
	}

	return true;
}

static bool find_by_filter(const void *a, const void *b)
{
	const struct pvt_rx_reg *rx_reg = a;
	const uint8_t *filter = b;

	return !memcmp(rx_reg->filter, filter, rx_reg->len);
}

static void scan_enable_rsp(const void *buf, uint8_t size,
							void *user_data)
{
	uint8_t status = *((uint8_t *) buf);

	if (status)
		l_error("LE Scan enable failed (0x%02x)", status);
}

static void set_recv_scan_enable(const void *buf, uint8_t size,
							void *user_data)
{
	struct mesh_io_private *pvt = user_data;
	struct bt_hci_cmd_le_set_scan_enable cmd;

	cmd.enable = 0x01;	/* Enable scanning */
	cmd.filter_dup = 0x00;	/* Report duplicates */
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
			&cmd, sizeof(cmd), scan_enable_rsp, pvt, NULL);
}

static void scan_disable_rsp(const void *buf, uint8_t size,
							void *user_data)
{
	struct bt_hci_cmd_le_set_scan_parameters cmd;
	struct mesh_io_private *pvt = user_data;
	uint8_t status = *((uint8_t *) buf);

	if (status)
		l_error("LE Scan disable failed (0x%02x)", status);

	cmd.type = pvt->active ? 0x01 : 0x00;	/* Passive/Active scanning */
	cmd.interval = L_CPU_TO_LE16(0x0010);	/* 10 ms */
	cmd.window = L_CPU_TO_LE16(0x0010);	/* 10 ms */
	cmd.own_addr_type = 0x01;		/* ADDR_TYPE_RANDOM */
	cmd.filter_policy = 0x00;		/* Accept all */

	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_PARAMETERS,
			&cmd, sizeof(cmd),
			set_recv_scan_enable, pvt, NULL);
}

static bool find_active(const void *a, const void *b)
{
	const struct pvt_rx_reg *rx_reg = a;

	/* Mesh specific AD types do *not* require active scanning,
	 * so do not turn on Active Scanning on their account.
	 */
	if (rx_reg->filter[0] < MESH_AD_TYPE_PROVISION ||
			rx_reg->filter[0] > MESH_AD_TYPE_BEACON)
		return true;

	return false;
}

static bool recv_register(struct mesh_io *io, const uint8_t *filter,
			uint8_t len, mesh_io_recv_func_t cb, void *user_data)
{
	struct bt_hci_cmd_le_set_scan_enable cmd;
	struct mesh_io_private *pvt = io->pvt;
	struct pvt_rx_reg *rx_reg;
	bool already_scanning;
	bool active = false;

	if (!cb || !filter || !len)
		return false;

	l_info("%s %2.2x", __func__, filter[0]);
	rx_reg = l_queue_remove_if(pvt->rx_regs, find_by_filter, filter);

	l_free(rx_reg);
	rx_reg = l_malloc(sizeof(*rx_reg) + len);

	memcpy(rx_reg->filter, filter, len);
	rx_reg->len = len;
	rx_reg->cb = cb;
	rx_reg->user_data = user_data;

	already_scanning = !l_queue_isempty(pvt->rx_regs);

	l_queue_push_head(pvt->rx_regs, rx_reg);

	/* Look for any AD types requiring Active Scanning */
	if (l_queue_find(pvt->rx_regs, find_active, NULL))
		active = true;

	if (!already_scanning || pvt->active != active) {
		pvt->active = active;
		cmd.enable = 0x00;	/* Disable scanning */
		cmd.filter_dup = 0x00;	/* Report duplicates */
		bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
				&cmd, sizeof(cmd), scan_disable_rsp, pvt, NULL);

	}

	return true;
}

static bool recv_deregister(struct mesh_io *io, const uint8_t *filter,
								uint8_t len)
{
	struct bt_hci_cmd_le_set_scan_enable cmd = {0, 0};
	struct mesh_io_private *pvt = io->pvt;
	struct pvt_rx_reg *rx_reg;
	bool active = false;

	rx_reg = l_queue_remove_if(pvt->rx_regs, find_by_filter, filter);

	if (rx_reg)
		l_free(rx_reg);

	/* Look for any AD types requiring Active Scanning */
	if (l_queue_find(pvt->rx_regs, find_active, NULL))
		active = true;

	if (l_queue_isempty(pvt->rx_regs)) {
		bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
					&cmd, sizeof(cmd), NULL, NULL, NULL);

	} else if (active != pvt->active) {
		pvt->active = active;
		bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
				&cmd, sizeof(cmd), scan_disable_rsp, pvt, NULL);
	}

	return true;
}

const struct mesh_io_api mesh_io_generic = {
	.init = dev_init,
	.destroy = dev_destroy,
	.caps = dev_caps,
	.send = send_tx,
	.reg = recv_register,
	.dereg = recv_deregister,
	.cancel = tx_cancel,
};
#ifdef CONFIG_XR829_BT
void set_scan_enable(struct mesh_io *io, bool enable)
{
	uint8_t state = enable ? 0x01 : 0x00;

	struct mesh_io_private *pvt = io->pvt;
	struct bt_hci_cmd_le_set_scan_enable cmd;
	cmd.enable = state;
	cmd.filter_dup = 0x00;
	bluez554_bt_hci_send(pvt->hci, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
			&cmd, sizeof(cmd), NULL, NULL, NULL);
}
#endif

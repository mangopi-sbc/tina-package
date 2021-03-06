/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2018  Intel Corporation. All rights reserved.
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

#define CONFIG_SRV_MODEL	(VENDOR_ID_MASK | 0x0000)
#define CONFIG_CLI_MODEL	(VENDOR_ID_MASK | 0x0001)

/* New List */
#define OP_APPKEY_ADD				0x00
#define OP_APPKEY_DELETE			0x8000
#define OP_APPKEY_GET				0x8001
#define OP_APPKEY_LIST				0x8002
#define OP_APPKEY_STATUS			0x8003
#define OP_APPKEY_UPDATE			0x01
#define OP_DEV_COMP_GET				0x8008
#define OP_DEV_COMP_STATUS			0x02
#define OP_CONFIG_BEACON_GET			0x8009
#define OP_CONFIG_BEACON_SET			0x800A
#define OP_CONFIG_BEACON_STATUS			0x800B
#define OP_CONFIG_DEFAULT_TTL_GET		0x800C
#define OP_CONFIG_DEFAULT_TTL_SET		0x800D
#define OP_CONFIG_DEFAULT_TTL_STATUS		0x800E
#define OP_CONFIG_FRIEND_GET			0x800F
#define OP_CONFIG_FRIEND_SET			0x8010
#define OP_CONFIG_FRIEND_STATUS			0x8011
#define OP_CONFIG_PROXY_GET			0x8012
#define OP_CONFIG_PROXY_SET			0x8013
#define OP_CONFIG_PROXY_STATUS			0x8014
#define OP_CONFIG_KEY_REFRESH_PHASE_GET		0x8015
#define OP_CONFIG_KEY_REFRESH_PHASE_SET		0x8016
#define OP_CONFIG_KEY_REFRESH_PHASE_STATUS	0x8017
#define OP_CONFIG_MODEL_PUB_GET			0x8018
#define OP_CONFIG_MODEL_PUB_SET			0x03
#define OP_CONFIG_MODEL_PUB_STATUS		0x8019
#define OP_CONFIG_MODEL_PUB_VIRT_SET		0x801A
#define OP_CONFIG_MODEL_SUB_ADD			0x801B
#define OP_CONFIG_MODEL_SUB_DELETE		0x801C
#define OP_CONFIG_MODEL_SUB_DELETE_ALL		0x801D
#define OP_CONFIG_MODEL_SUB_OVERWRITE		0x801E
#define OP_CONFIG_MODEL_SUB_STATUS		0x801F
#define OP_CONFIG_MODEL_SUB_VIRT_ADD		0x8020
#define OP_CONFIG_MODEL_SUB_VIRT_DELETE		0x8021
#define OP_CONFIG_MODEL_SUB_VIRT_OVERWRITE	0x8022
#define OP_CONFIG_NETWORK_TRANSMIT_GET		0x8023
#define OP_CONFIG_NETWORK_TRANSMIT_SET		0x8024
#define OP_CONFIG_NETWORK_TRANSMIT_STATUS	0x8025
#define OP_CONFIG_RELAY_GET			0x8026
#define OP_CONFIG_RELAY_SET			0x8027
#define OP_CONFIG_RELAY_STATUS			0x8028
#define OP_CONFIG_MODEL_SUB_GET			0x8029
#define OP_CONFIG_MODEL_SUB_LIST		0x802A
#define OP_CONFIG_VEND_MODEL_SUB_GET		0x802B
#define OP_CONFIG_VEND_MODEL_SUB_LIST		0x802C
#define OP_CONFIG_POLL_TIMEOUT_LIST		0x802D
#define OP_CONFIG_POLL_TIMEOUT_STATUS		0x802E
/* Health opcodes in health-mod.h */
#define OP_CONFIG_HEARTBEAT_PUB_GET		0x8038
#define OP_CONFIG_HEARTBEAT_PUB_SET		0x8039
#define OP_CONFIG_HEARTBEAT_PUB_STATUS		0x06
#define OP_CONFIG_HEARTBEAT_SUB_GET		0x803A
#define OP_CONFIG_HEARTBEAT_SUB_SET		0x803B
#define OP_CONFIG_HEARTBEAT_SUB_STATUS		0x803C
#define OP_MODEL_APP_BIND			0x803D
#define OP_MODEL_APP_STATUS			0x803E
#define OP_MODEL_APP_UNBIND			0x803F
#define OP_NETKEY_ADD				0x8040
#define OP_NETKEY_DELETE			0x8041
#define OP_NETKEY_GET				0x8042
#define OP_NETKEY_LIST				0x8043
#define OP_NETKEY_STATUS			0x8044
#define OP_NETKEY_UPDATE			0x8045
#define OP_NODE_IDENTITY_GET			0x8046
#define OP_NODE_IDENTITY_SET			0x8047
#define OP_NODE_IDENTITY_STATUS			0x8048
#define OP_NODE_RESET				0x8049
#define OP_NODE_RESET_STATUS			0x804A
#define OP_MODEL_APP_GET			0x804B
#define OP_MODEL_APP_LIST			0x804C
#define OP_VEND_MODEL_APP_GET			0x804D
#define OP_VEND_MODEL_APP_LIST			0x804E

void cfgmod_server_init(struct mesh_node *node, uint8_t ele_idx);
#ifdef CONFIG_XR829_BT
void mesh_config_srv_init(struct mesh_node *node, uint8_t ele_idx);
void cfgmod_cli_init(struct mesh_node *node, uint8_t ele_idx);
bool mesh_config_cli_msg_handler(struct mesh_node *node, uint16_t src, uint16_t dst,uint16_t global_net_idx,
			uint8_t ttl, const void *data, uint32_t len);
#endif

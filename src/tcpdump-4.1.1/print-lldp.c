/*
 * Copyright (c) 1998-2007 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * support for the IEEE Link Discovery Protocol as per 802.1ab
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 * IEEE and TIA extensions by Carles Kishimoto <carles.kishimoto@gmail.com>
 */

#ifndef lint
static const char rcsid[] _U_ =
"@(#) $Header$";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "af.h"
#include "oui.h"

#define	LLDP_EXTRACT_TYPE(x) (((x)&0xfe00)>>9) 
#define	LLDP_EXTRACT_LEN(x) ((x)&0x01ff) 

/*
 * TLV type codes
 */
#define LLDP_END_TLV             0
#define LLDP_CHASSIS_ID_TLV      1
#define LLDP_PORT_ID_TLV         2
#define LLDP_TTL_TLV             3
#define LLDP_PORT_DESCR_TLV      4
#define LLDP_SYSTEM_NAME_TLV     5
#define LLDP_SYSTEM_DESCR_TLV    6
#define LLDP_SYSTEM_CAP_TLV      7
#define LLDP_MGMT_ADDR_TLV       8
#define LLDP_PRIVATE_TLV       127

static const struct tok lldp_tlv_values[] = {
    { LLDP_END_TLV, "End" },
    { LLDP_CHASSIS_ID_TLV, "Chassis ID" },
    { LLDP_PORT_ID_TLV, "Port ID" },
    { LLDP_TTL_TLV, "Time to Live" },
    { LLDP_PORT_DESCR_TLV, "Port Description" },
    { LLDP_SYSTEM_NAME_TLV, "System Name" },
    { LLDP_SYSTEM_DESCR_TLV, "System Description" },
    { LLDP_SYSTEM_CAP_TLV, "System Capabilities" },
    { LLDP_MGMT_ADDR_TLV, "Management Address" },
    { LLDP_PRIVATE_TLV, "Organization specific" },
    { 0, NULL}
};

/*
 * Chassis ID subtypes
 */
#define LLDP_CHASSIS_CHASSIS_COMP_SUBTYPE  1
#define LLDP_CHASSIS_INTF_ALIAS_SUBTYPE    2
#define LLDP_CHASSIS_PORT_COMP_SUBTYPE     3
#define LLDP_CHASSIS_MAC_ADDR_SUBTYPE      4
#define LLDP_CHASSIS_NETWORK_ADDR_SUBTYPE  5
#define LLDP_CHASSIS_INTF_NAME_SUBTYPE     6
#define LLDP_CHASSIS_LOCAL_SUBTYPE         7

static const struct tok lldp_chassis_subtype_values[] = {
    { LLDP_CHASSIS_CHASSIS_COMP_SUBTYPE, "Chassis component"},
    { LLDP_CHASSIS_INTF_ALIAS_SUBTYPE, "Interface alias"},
    { LLDP_CHASSIS_PORT_COMP_SUBTYPE, "Port component"},
    { LLDP_CHASSIS_MAC_ADDR_SUBTYPE, "MAC address"},
    { LLDP_CHASSIS_NETWORK_ADDR_SUBTYPE, "Network address"},
    { LLDP_CHASSIS_INTF_NAME_SUBTYPE, "Interface name"},
    { LLDP_CHASSIS_LOCAL_SUBTYPE, "Local"},
    { 0, NULL}
};

/*
 * Port ID subtypes
 */
#define LLDP_PORT_INTF_ALIAS_SUBTYPE       1
#define LLDP_PORT_PORT_COMP_SUBTYPE        2
#define LLDP_PORT_MAC_ADDR_SUBTYPE         3
#define LLDP_PORT_NETWORK_ADDR_SUBTYPE     4
#define LLDP_PORT_INTF_NAME_SUBTYPE        5
#define LLDP_PORT_AGENT_CIRC_ID_SUBTYPE    6
#define LLDP_PORT_LOCAL_SUBTYPE            7

static const struct tok lldp_port_subtype_values[] = {
    { LLDP_PORT_INTF_ALIAS_SUBTYPE, "Interface alias"},
    { LLDP_PORT_PORT_COMP_SUBTYPE, "Port component"},
    { LLDP_PORT_MAC_ADDR_SUBTYPE, "MAC address"},
    { LLDP_PORT_NETWORK_ADDR_SUBTYPE, "Network Address"},
    { LLDP_PORT_INTF_NAME_SUBTYPE, "Interface Name"},
    { LLDP_PORT_AGENT_CIRC_ID_SUBTYPE, "Agent circuit ID"},
    { LLDP_PORT_LOCAL_SUBTYPE, "Local"},
    { 0, NULL}
};

/*
 * System Capabilities
 */
#define LLDP_CAP_OTHER              (1 <<  0)
#define LLDP_CAP_REPEATER           (1 <<  1)
#define LLDP_CAP_BRIDGE             (1 <<  2)
#define LLDP_CAP_WLAN_AP            (1 <<  3)
#define LLDP_CAP_ROUTER             (1 <<  4)
#define LLDP_CAP_PHONE              (1 <<  5)
#define LLDP_CAP_DOCSIS             (1 <<  6)
#define LLDP_CAP_STATION_ONLY       (1 <<  7)

static const struct tok lldp_cap_values[] = {
    { LLDP_CAP_OTHER, "Other"},
    { LLDP_CAP_REPEATER, "Repeater"},
    { LLDP_CAP_BRIDGE, "Bridge"},
    { LLDP_CAP_WLAN_AP, "WLAN AP"},
    { LLDP_CAP_ROUTER, "Router"},
    { LLDP_CAP_PHONE, "Telephone"},
    { LLDP_CAP_DOCSIS, "Docsis"},
    { LLDP_CAP_STATION_ONLY, "Station Only"},
    { 0, NULL}
};

#define LLDP_PRIVATE_8021_SUBTYPE_PORT_VLAN_ID		1
#define LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_VLAN_ID	2
#define LLDP_PRIVATE_8021_SUBTYPE_VLAN_NAME		3
#define LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_IDENTITY	4

static const struct tok lldp_8021_subtype_values[] = {
    { LLDP_PRIVATE_8021_SUBTYPE_PORT_VLAN_ID, "Port VLAN Id"},
    { LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_VLAN_ID, "Port and Protocol VLAN ID"},
    { LLDP_PRIVATE_8021_SUBTYPE_VLAN_NAME, "VLAN name"},
    { LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_IDENTITY, "Protocol Identity"},
    { 0, NULL}
};

#define LLDP_8021_PORT_PROTOCOL_VLAN_SUPPORT       (1 <<  1)
#define LLDP_8021_PORT_PROTOCOL_VLAN_STATUS        (1 <<  2)

static const struct tok lldp_8021_port_protocol_id_values[] = {
    { LLDP_8021_PORT_PROTOCOL_VLAN_SUPPORT, "supported"},
    { LLDP_8021_PORT_PROTOCOL_VLAN_STATUS, "enabled"},
    { 0, NULL}
};

#define LLDP_PRIVATE_8023_SUBTYPE_MACPHY        1
#define LLDP_PRIVATE_8023_SUBTYPE_MDIPOWER      2
#define LLDP_PRIVATE_8023_SUBTYPE_LINKAGGR      3
#define LLDP_PRIVATE_8023_SUBTYPE_MTU           4

static const struct tok lldp_8023_subtype_values[] = {
    { LLDP_PRIVATE_8023_SUBTYPE_MACPHY,	"MAC/PHY configuration/status"},
    { LLDP_PRIVATE_8023_SUBTYPE_MDIPOWER, "Power via MDI"},
    { LLDP_PRIVATE_8023_SUBTYPE_LINKAGGR, "Link aggregation"},
    { LLDP_PRIVATE_8023_SUBTYPE_MTU, "Max frame size"},
    { 0, NULL}
};

#define LLDP_PRIVATE_TIA_SUBTYPE_CAPABILITIES                   1
#define LLDP_PRIVATE_TIA_SUBTYPE_NETWORK_POLICY                 2
#define LLDP_PRIVATE_TIA_SUBTYPE_LOCAL_ID                       3
#define LLDP_PRIVATE_TIA_SUBTYPE_EXTENDED_POWER_MDI             4
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_HARDWARE_REV         5
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_FIRMWARE_REV         6
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SOFTWARE_REV         7
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SERIAL_NUMBER        8
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MANUFACTURER_NAME    9
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MODEL_NAME           10
#define LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_ASSET_ID             11

static const struct tok lldp_tia_subtype_values[] = {
    { LLDP_PRIVATE_TIA_SUBTYPE_CAPABILITIES, "LLDP-MED Capabilities" },
    { LLDP_PRIVATE_TIA_SUBTYPE_NETWORK_POLICY, "Network policy" },
    { LLDP_PRIVATE_TIA_SUBTYPE_LOCAL_ID, "Location identification" },
    { LLDP_PRIVATE_TIA_SUBTYPE_EXTENDED_POWER_MDI, "Extended power-via-MDI" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_HARDWARE_REV, "Inventory - hardware revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_FIRMWARE_REV, "Inventory - firmware revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SOFTWARE_REV, "Inventory - software revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SERIAL_NUMBER, "Inventory - serial number" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MANUFACTURER_NAME, "Inventory - manufacturer name" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MODEL_NAME, "Inventory - model name" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_ASSET_ID, "Inventory - asset ID" },
    { 0, NULL}
};

#define LLDP_PRIVATE_TIA_LOCATION_ALTITUDE_METERS       1
#define LLDP_PRIVATE_TIA_LOCATION_ALTITUDE_FLOORS       2

static const struct tok lldp_tia_location_altitude_type_values[] = {
    { LLDP_PRIVATE_TIA_LOCATION_ALTITUDE_METERS, "meters"},
    { LLDP_PRIVATE_TIA_LOCATION_ALTITUDE_FLOORS, "floors"},
    { 0, NULL}
};

/* ANSI/TIA-1057 - Annex B */
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A1		1
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A2		2
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A3		3
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A4		4
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A5		5
#define LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A6		6

static const struct tok lldp_tia_location_lci_catype_values[] = {
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A1, "national subdivisions (state,canton,region,province,prefecture)"},
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A2, "county, parish, gun, district"},
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A3, "city, township, shi"},
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A4, "city division, borough, city district, ward chou"},
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A5, "neighborhood, block"},
    { LLDP_PRIVATE_TIA_LOCATION_LCI_CATYPE_A6, "street"},
    { 0, NULL}
};

static const struct tok lldp_tia_location_lci_what_values[] = {
    { 0, "location of DHCP server"},
    { 1, "location of the network element believed to be closest to the client"}, 
    { 2, "location of the client"},
    { 0, NULL}
};

/*
 * From RFC 3636 - dot3MauType
 */
#define		LLDP_MAU_TYPE_UNKNOWN		0
#define		LLDP_MAU_TYPE_AUI		1
#define		LLDP_MAU_TYPE_10BASE_5		2
#define		LLDP_MAU_TYPE_FOIRL		3
#define		LLDP_MAU_TYPE_10BASE_2		4
#define		LLDP_MAU_TYPE_10BASE_T		5
#define		LLDP_MAU_TYPE_10BASE_FP		6
#define		LLDP_MAU_TYPE_10BASE_FB		7
#define		LLDP_MAU_TYPE_10BASE_FL		8
#define		LLDP_MAU_TYPE_10BROAD36		9
#define		LLDP_MAU_TYPE_10BASE_T_HD	10
#define		LLDP_MAU_TYPE_10BASE_T_FD	11
#define		LLDP_MAU_TYPE_10BASE_FL_HD	12
#define		LLDP_MAU_TYPE_10BASE_FL_FD	13
#define		LLDP_MAU_TYPE_100BASE_T4	14
#define		LLDP_MAU_TYPE_100BASE_TX_HD	15
#define		LLDP_MAU_TYPE_100BASE_TX_FD	16
#define		LLDP_MAU_TYPE_100BASE_FX_HD	17
#define		LLDP_MAU_TYPE_100BASE_FX_FD	18
#define		LLDP_MAU_TYPE_100BASE_T2_HD	19
#define		LLDP_MAU_TYPE_100BASE_T2_FD	20
#define		LLDP_MAU_TYPE_1000BASE_X_HD	21
#define		LLDP_MAU_TYPE_1000BASE_X_FD	22
#define		LLDP_MAU_TYPE_1000BASE_LX_HD	23
#define		LLDP_MAU_TYPE_1000BASE_LX_FD	24
#define		LLDP_MAU_TYPE_1000BASE_SX_HD	25
#define		LLDP_MAU_TYPE_1000BASE_SX_FD	26
#define		LLDP_MAU_TYPE_1000BASE_CX_HD	27
#define		LLDP_MAU_TYPE_1000BASE_CX_FD	28
#define		LLDP_MAU_TYPE_1000BASE_T_HD	29
#define		LLDP_MAU_TYPE_1000BASE_T_FD	30
#define		LLDP_MAU_TYPE_10GBASE_X		31
#define		LLDP_MAU_TYPE_10GBASE_LX4	32
#define		LLDP_MAU_TYPE_10GBASE_R		33
#define		LLDP_MAU_TYPE_10GBASE_ER	34
#define		LLDP_MAU_TYPE_10GBASE_LR	35
#define		LLDP_MAU_TYPE_10GBASE_SR	36
#define		LLDP_MAU_TYPE_10GBASE_W		37
#define		LLDP_MAU_TYPE_10GBASE_EW	38
#define		LLDP_MAU_TYPE_10GBASE_LW	39
#define		LLDP_MAU_TYPE_10GBASE_SW	40

static const struct tok lldp_mau_types_values[] = {
    { LLDP_MAU_TYPE_UNKNOWN,            "Unknown"},
    { LLDP_MAU_TYPE_AUI,                "AUI"},
    { LLDP_MAU_TYPE_10BASE_5,           "10BASE_5"},
    { LLDP_MAU_TYPE_FOIRL,              "FOIRL"},
    { LLDP_MAU_TYPE_10BASE_2,           "10BASE2"},
    { LLDP_MAU_TYPE_10BASE_T,           "10BASET duplex mode unknown"},
    { LLDP_MAU_TYPE_10BASE_FP,          "10BASEFP"},
    { LLDP_MAU_TYPE_10BASE_FB,          "10BASEFB"},
    { LLDP_MAU_TYPE_10BASE_FL,          "10BASEFL duplex mode unknown"},
    { LLDP_MAU_TYPE_10BROAD36,          "10BROAD36"},
    { LLDP_MAU_TYPE_10BASE_T_HD,        "10BASET hdx"},
    { LLDP_MAU_TYPE_10BASE_T_FD,        "10BASET fdx"},
    { LLDP_MAU_TYPE_10BASE_FL_HD,       "10BASEFL hdx"},
    { LLDP_MAU_TYPE_10BASE_FL_FD,       "10BASEFL fdx"},
    { LLDP_MAU_TYPE_100BASE_T4,         "100BASET4"},
    { LLDP_MAU_TYPE_100BASE_TX_HD,      "100BASETX hdx"},
    { LLDP_MAU_TYPE_100BASE_TX_FD,      "100BASETX fdx"},
    { LLDP_MAU_TYPE_100BASE_FX_HD,      "100BASEFX hdx"},
    { LLDP_MAU_TYPE_100BASE_FX_FD,      "100BASEFX fdx"},
    { LLDP_MAU_TYPE_100BASE_T2_HD,      "100BASET2 hdx"},
    { LLDP_MAU_TYPE_100BASE_T2_FD,      "100BASET2 fdx"},
    { LLDP_MAU_TYPE_1000BASE_X_HD,      "1000BASEX hdx"},
    { LLDP_MAU_TYPE_1000BASE_X_FD,      "1000BASEX fdx"},
    { LLDP_MAU_TYPE_1000BASE_LX_HD,     "1000BASELX hdx"},
    { LLDP_MAU_TYPE_1000BASE_LX_FD,     "1000BASELX fdx"},
    { LLDP_MAU_TYPE_1000BASE_SX_HD,     "1000BASESX hdx"},
    { LLDP_MAU_TYPE_1000BASE_SX_FD,     "1000BASESX fdx"},
    { LLDP_MAU_TYPE_1000BASE_CX_HD,     "1000BASECX hdx"},
    { LLDP_MAU_TYPE_1000BASE_CX_FD,     "1000BASECX fdx"},
    { LLDP_MAU_TYPE_1000BASE_T_HD,      "1000BASET hdx"},
    { LLDP_MAU_TYPE_1000BASE_T_FD,      "1000BASET fdx"},
    { LLDP_MAU_TYPE_10GBASE_X,          "10GBASEX"},
    { LLDP_MAU_TYPE_10GBASE_LX4,        "10GBASELX4"},
    { LLDP_MAU_TYPE_10GBASE_R,          "10GBASER"},
    { LLDP_MAU_TYPE_10GBASE_ER,         "10GBASEER"},
    { LLDP_MAU_TYPE_10GBASE_LR,         "10GBASELR"},
    { LLDP_MAU_TYPE_10GBASE_SR,         "10GBASESR"},
    { LLDP_MAU_TYPE_10GBASE_W,          "10GBASEW"},
    { LLDP_MAU_TYPE_10GBASE_EW,         "10GBASEEW"},
    { LLDP_MAU_TYPE_10GBASE_LW,         "10GBASELW"},
    { LLDP_MAU_TYPE_10GBASE_SW,         "10GBASESW"},
    { 0, NULL}
};

#define LLDP_8023_AUTONEGOTIATION_SUPPORT       (1 <<  0)
#define LLDP_8023_AUTONEGOTIATION_STATUS        (1 <<  1)

static const struct tok lldp_8023_autonegotiation_values[] = {
    { LLDP_8023_AUTONEGOTIATION_SUPPORT, "supported"},
    { LLDP_8023_AUTONEGOTIATION_STATUS, "enabled"},
    { 0, NULL}
};

#define LLDP_TIA_CAPABILITY_MED                         (1 <<  0)
#define LLDP_TIA_CAPABILITY_NETWORK_POLICY              (1 <<  1)
#define LLDP_TIA_CAPABILITY_LOCATION_IDENTIFICATION     (1 <<  2)
#define LLDP_TIA_CAPABILITY_EXTENDED_POWER_MDI_PSE      (1 <<  3)
#define LLDP_TIA_CAPABILITY_EXTENDED_POWER_MDI_PD       (1 <<  4)
#define LLDP_TIA_CAPABILITY_INVENTORY                   (1 <<  5)

static const struct tok lldp_tia_capabilities_values[] = {
    { LLDP_TIA_CAPABILITY_MED, "LLDP-MED capabilities"},
    { LLDP_TIA_CAPABILITY_NETWORK_POLICY, "network policy"},
    { LLDP_TIA_CAPABILITY_LOCATION_IDENTIFICATION, "location identification"},
    { LLDP_TIA_CAPABILITY_EXTENDED_POWER_MDI_PSE, "extended power via MDI-PSE"},
    { LLDP_TIA_CAPABILITY_EXTENDED_POWER_MDI_PD, "extended power via MDI-PD"},
    { LLDP_TIA_CAPABILITY_INVENTORY, "Inventory"},
    { 0, NULL}
};

#define LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_1           1
#define LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_2           2
#define LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_3           3
#define LLDP_TIA_DEVICE_TYPE_NETWORK_CONNECTIVITY       4

static const struct tok lldp_tia_device_type_values[] = {
    { LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_1, "endpoint class 1"},
    { LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_2, "endpoint class 2"},
    { LLDP_TIA_DEVICE_TYPE_ENDPOINT_CLASS_3, "endpoint class 3"},
    { LLDP_TIA_DEVICE_TYPE_NETWORK_CONNECTIVITY, "network connectivity"},
    { 0, NULL}
};

#define LLDP_TIA_APPLICATION_TYPE_VOICE                 1
#define LLDP_TIA_APPLICATION_TYPE_VOICE_SIGNALING       2
#define LLDP_TIA_APPLICATION_TYPE_GUEST_VOICE           3
#define LLDP_TIA_APPLICATION_TYPE_GUEST_VOICE_SIGNALING 4
#define LLDP_TIA_APPLICATION_TYPE_SOFTPHONE_VOICE       5
#define LLDP_TIA_APPLICATION_TYPE_VIDEO_CONFERENCING    6
#define LLDP_TIA_APPLICATION_TYPE_STREAMING_VIDEO       7
#define LLDP_TIA_APPLICATION_TYPE_VIDEO_SIGNALING       8

static const struct tok lldp_tia_application_type_values[] = {
    { LLDP_TIA_APPLICATION_TYPE_VOICE, "voice"},
    { LLDP_TIA_APPLICATION_TYPE_VOICE_SIGNALING, "voice signaling"},
    { LLDP_TIA_APPLICATION_TYPE_GUEST_VOICE, "guest voice"},
    { LLDP_TIA_APPLICATION_TYPE_GUEST_VOICE_SIGNALING, "guest voice signaling"},
    { LLDP_TIA_APPLICATION_TYPE_SOFTPHONE_VOICE, "softphone voice"},
    { LLDP_TIA_APPLICATION_TYPE_VIDEO_CONFERENCING, "video conferencing"},
    { LLDP_TIA_APPLICATION_TYPE_STREAMING_VIDEO, "streaming video"},
    { LLDP_TIA_APPLICATION_TYPE_VIDEO_SIGNALING, "video signaling"},
    { 0, NULL}
};

#define LLDP_TIA_NETWORK_POLICY_U_BIT           (1 << 5)
#define LLDP_TIA_NETWORK_POLICY_T_BIT           (1 << 6)
#define LLDP_TIA_NETWORK_POLICY_X_BIT           (1 << 7)

static const struct tok lldp_tia_network_policy_bits_values[] = {
    { LLDP_TIA_NETWORK_POLICY_U_BIT, "Unknown"},
    { LLDP_TIA_NETWORK_POLICY_T_BIT, "Tagged"},
    { LLDP_TIA_NETWORK_POLICY_X_BIT, "reserved"},
    { 0, NULL}
};

#define LLDP_EXTRACT_NETWORK_POLICY_VLAN(x)           (((x)&0x1ffe)>>1)
#define LLDP_EXTRACT_NETWORK_POLICY_L2_PRIORITY(x)    (((x)&0x01ff)>>6)
#define LLDP_EXTRACT_NETWORK_POLICY_DSCP(x)           ((x)&0x003f)

#define LLDP_TIA_LOCATION_DATA_FORMAT_COORDINATE_BASED  1
#define LLDP_TIA_LOCATION_DATA_FORMAT_CIVIC_ADDRESS     2
#define LLDP_TIA_LOCATION_DATA_FORMAT_ECS_ELIN          3

static const struct tok lldp_tia_location_data_format_values[] = {
    { LLDP_TIA_LOCATION_DATA_FORMAT_COORDINATE_BASED, "coordinate-based LCI"},
    { LLDP_TIA_LOCATION_DATA_FORMAT_CIVIC_ADDRESS, "civic address LCI"},
    { LLDP_TIA_LOCATION_DATA_FORMAT_ECS_ELIN, "ECS ELIN"},
    { 0, NULL}
};

#define LLDP_TIA_LOCATION_DATUM_WGS_84          1
#define LLDP_TIA_LOCATION_DATUM_NAD_83_NAVD_88  2
#define LLDP_TIA_LOCATION_DATUM_NAD_83_MLLW     3

static const struct tok lldp_tia_location_datum_type_values[] = {
    { LLDP_TIA_LOCATION_DATUM_WGS_84, "World Geodesic System 1984"},
    { LLDP_TIA_LOCATION_DATUM_NAD_83_NAVD_88, "North American Datum 1983 (NAVD88)"},
    { LLDP_TIA_LOCATION_DATUM_NAD_83_MLLW, "North American Datum 1983 (MLLW)"},
    { 0, NULL}
};

#define LLDP_TIA_POWER_SOURCE_PSE               1
#define LLDP_TIA_POWER_SOURCE_LOCAL             2
#define LLDP_TIA_POWER_SOURCE_PSE_AND_LOCAL     3

static const struct tok lldp_tia_power_source_values[] = {
    { LLDP_TIA_POWER_SOURCE_PSE, "PSE - primary power source"},
    { LLDP_TIA_POWER_SOURCE_LOCAL, "local - backup power source"},
    { LLDP_TIA_POWER_SOURCE_PSE_AND_LOCAL, "PSE+local - reserved"},
    { 0, NULL}
};

#define LLDP_TIA_POWER_PRIORITY_CRITICAL        1
#define LLDP_TIA_POWER_PRIORITY_HIGH            2
#define LLDP_TIA_POWER_PRIORITY_LOW             3

static const struct tok lldp_tia_power_priority_values[] = {
    { LLDP_TIA_POWER_PRIORITY_CRITICAL, "critical"},
    { LLDP_TIA_POWER_PRIORITY_HIGH, "high"},
    { LLDP_TIA_POWER_PRIORITY_LOW, "low"},
    { 0, NULL}
};

#define LLDP_TIA_POWER_VAL_MAX               1024

static const struct tok lldp_tia_inventory_values[] = {
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_HARDWARE_REV, "Hardware revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_FIRMWARE_REV, "Firmware revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SOFTWARE_REV, "Software revision" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SERIAL_NUMBER, "Serial number" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MANUFACTURER_NAME, "Manufacturer name" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MODEL_NAME, "Model name" },
    { LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_ASSET_ID, "Asset ID" },
    { 0, NULL}
};

/*
 * From RFC 3636 - ifMauAutoNegCapAdvertisedBits
 */ 
#define	 LLDP_MAU_PMD_OTHER			(1 <<  15)
#define	 LLDP_MAU_PMD_10BASE_T			(1 <<  14)
#define	 LLDP_MAU_PMD_10BASE_T_FD		(1 <<  13)
#define	 LLDP_MAU_PMD_100BASE_T4		(1 <<  12)
#define	 LLDP_MAU_PMD_100BASE_TX		(1 <<  11)
#define	 LLDP_MAU_PMD_100BASE_TX_FD		(1 <<  10)
#define	 LLDP_MAU_PMD_100BASE_T2		(1 <<  9)
#define	 LLDP_MAU_PMD_100BASE_T2_FD		(1 <<  8)
#define	 LLDP_MAU_PMD_FDXPAUSE			(1 <<  7)
#define	 LLDP_MAU_PMD_FDXAPAUSE			(1 <<  6)
#define	 LLDP_MAU_PMD_FDXSPAUSE			(1 <<  5)
#define	 LLDP_MAU_PMD_FDXBPAUSE			(1 <<  4)
#define	 LLDP_MAU_PMD_1000BASE_X		(1 <<  3)
#define	 LLDP_MAU_PMD_1000BASE_X_FD		(1 <<  2)
#define	 LLDP_MAU_PMD_1000BASE_T		(1 <<  1)
#define	 LLDP_MAU_PMD_1000BASE_T_FD		(1 <<  0)

static const struct tok lldp_pmd_capability_values[] = {
    { LLDP_MAU_PMD_10BASE_T,		"10BASE-T hdx"},
    { LLDP_MAU_PMD_10BASE_T_FD,	        "10BASE-T fdx"},
    { LLDP_MAU_PMD_100BASE_T4,		"100BASE-T4"},
    { LLDP_MAU_PMD_100BASE_TX,		"100BASE-TX hdx"},
    { LLDP_MAU_PMD_100BASE_TX_FD,	"100BASE-TX fdx"},
    { LLDP_MAU_PMD_100BASE_T2,		"100BASE-T2 hdx"},
    { LLDP_MAU_PMD_100BASE_T2_FD,	"100BASE-T2 fdx"},
    { LLDP_MAU_PMD_FDXPAUSE,		"Pause for fdx links"},
    { LLDP_MAU_PMD_FDXAPAUSE,		"Asym PAUSE for fdx"},
    { LLDP_MAU_PMD_FDXSPAUSE,		"Sym PAUSE for fdx"},
    { LLDP_MAU_PMD_FDXBPAUSE,		"Asym and Sym PAUSE for fdx"},
    { LLDP_MAU_PMD_1000BASE_X,		"1000BASE-{X LX SX CX} hdx"},
    { LLDP_MAU_PMD_1000BASE_X_FD,	"1000BASE-{X LX SX CX} fdx"},
    { LLDP_MAU_PMD_1000BASE_T,		"1000BASE-T hdx"},
    { LLDP_MAU_PMD_1000BASE_T_FD,	"1000BASE-T fdx"},
    { 0, NULL}
};

#define	LLDP_MDI_PORT_CLASS			(1 <<  0)
#define	LLDP_MDI_POWER_SUPPORT			(1 <<  1)
#define LLDP_MDI_POWER_STATE			(1 <<  2)
#define LLDP_MDI_PAIR_CONTROL_ABILITY		(1 <<  3)

static const struct tok lldp_mdi_values[] = {
    { LLDP_MDI_PORT_CLASS, 		"PSE"},
    { LLDP_MDI_POWER_SUPPORT, 		"supported"},
    { LLDP_MDI_POWER_STATE, 		"enabled"},
    { LLDP_MDI_PAIR_CONTROL_ABILITY, 	"can be controlled"},
    { 0, NULL}
};

#define LLDP_MDI_PSE_PORT_POWER_PAIRS_SIGNAL	1
#define LLDP_MDI_PSE_PORT_POWER_PAIRS_SPARE	2

static const struct tok lldp_mdi_power_pairs_values[] = {
    { LLDP_MDI_PSE_PORT_POWER_PAIRS_SIGNAL,	"signal"},
    { LLDP_MDI_PSE_PORT_POWER_PAIRS_SPARE,	"spare"},
    { 0, NULL}
};

#define LLDP_MDI_POWER_CLASS0		1
#define LLDP_MDI_POWER_CLASS1		2
#define LLDP_MDI_POWER_CLASS2		3
#define LLDP_MDI_POWER_CLASS3		4
#define LLDP_MDI_POWER_CLASS4		5

static const struct tok lldp_mdi_power_class_values[] = {
    { LLDP_MDI_POWER_CLASS0,     "class0"},
    { LLDP_MDI_POWER_CLASS1,     "class1"},
    { LLDP_MDI_POWER_CLASS2,     "class2"},
    { LLDP_MDI_POWER_CLASS3,     "class3"},
    { LLDP_MDI_POWER_CLASS4,     "class4"},
    { 0, NULL}
};

#define LLDP_AGGREGATION_CAPABILTIY     (1 <<  0)
#define LLDP_AGGREGATION_STATUS         (1 <<  1)

static const struct tok lldp_aggregation_values[] = {
    { LLDP_AGGREGATION_CAPABILTIY, "supported"},
    { LLDP_AGGREGATION_STATUS, "enabled"},
    { 0, NULL}
};

/*
 * Interface numbering subtypes.
 */
#define LLDP_INTF_NUMB_IFX_SUBTYPE         2
#define LLDP_INTF_NUMB_SYSPORT_SUBTYPE     3

static const struct tok lldp_intf_numb_subtype_values[] = {
    { LLDP_INTF_NUMB_IFX_SUBTYPE, "Interface Index" },
    { LLDP_INTF_NUMB_SYSPORT_SUBTYPE, "System Port Number" },
    { 0, NULL}
};

#define LLDP_INTF_NUM_LEN                  5

/*
 * Print IEEE private extensions. (802.1 annex F)
 */
static int
lldp_private_8021_print(const u_char *tptr)
{
    int subtype, hexdump = FALSE;

    subtype = *(tptr+3);

    printf("\n\t  %s Subtype (%u)",
           tok2str(lldp_8021_subtype_values, "unknown", subtype),
           subtype);

    switch (subtype) {
    case LLDP_PRIVATE_8021_SUBTYPE_PORT_VLAN_ID:
        printf("\n\t    port vlan id (PVID): %u",
               EXTRACT_16BITS(tptr+4));
        break;
    case LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_VLAN_ID:
        printf("\n\t    port and protocol vlan id (PPVID): %u, flags [%s] (0x%02x)",
               EXTRACT_16BITS(tptr+5),
	       bittok2str(lldp_8021_port_protocol_id_values, "none", *(tptr+4)),
	       *(tptr+4));
        break;
    case LLDP_PRIVATE_8021_SUBTYPE_VLAN_NAME:
        printf("\n\t    vlan id (VID): %u",
               EXTRACT_16BITS(tptr+4));
        printf("\n\t    vlan name: ");
        safeputs((const char *)tptr+7, *(tptr+6));
        break;
    case LLDP_PRIVATE_8021_SUBTYPE_PROTOCOL_IDENTITY:
        printf("\n\t    protocol identity: ");
        safeputs((const char *)tptr+5, *(tptr+4));
        break;

    default:
        hexdump = TRUE;
        break;
    }

    return hexdump;
}

/*
 * Print IEEE private extensions. (802.3)
 */
static int
lldp_private_8023_print(const u_char *tptr)
{
    int subtype, hexdump = FALSE;

    subtype = *(tptr+3);

    printf("\n\t  %s Subtype (%u)",
           tok2str(lldp_8023_subtype_values, "unknown", subtype),
           subtype);

    switch (subtype) {
    case LLDP_PRIVATE_8023_SUBTYPE_MACPHY:
        printf("\n\t    autonegotiation [%s] (0x%02x)",
               bittok2str(lldp_8023_autonegotiation_values, "none", *(tptr+4)),
               *(tptr+4));
        printf("\n\t    PMD autoneg capability [%s] (0x%04x)",
               bittok2str(lldp_pmd_capability_values,"unknown", EXTRACT_16BITS(tptr+5)),
               EXTRACT_16BITS(tptr+5));
        printf("\n\t    MAU type %s (0x%04x)",
               tok2str(lldp_mau_types_values, "unknown", EXTRACT_16BITS(tptr+7)),
               EXTRACT_16BITS(tptr+7));
        break;

    case LLDP_PRIVATE_8023_SUBTYPE_MDIPOWER:
        printf("\n\t    MDI power support [%s], power pair %s, power class %s",
               bittok2str(lldp_mdi_values, "none", *(tptr+4)),
               tok2str(lldp_mdi_power_pairs_values, "unknown", *(tptr+5)),
               tok2str(lldp_mdi_power_class_values, "unknown", *(tptr+6)));
        break;

    case LLDP_PRIVATE_8023_SUBTYPE_LINKAGGR:
        printf("\n\t    aggregation status [%s], aggregation port ID %u",
               bittok2str(lldp_aggregation_values, "none", *(tptr+4)),
               EXTRACT_32BITS(tptr+5));
        break;

    case LLDP_PRIVATE_8023_SUBTYPE_MTU:
        printf("\n\t    MTU size %u", EXTRACT_16BITS(tptr+4));
        break;

    default:
        hexdump = TRUE;
        break;
    }

    return hexdump;
}

/*
 * Extract 34bits of latitude/longitude coordinates.
 */
static u_int64_t
lldp_extract_latlon(const u_char *tptr)
{
    u_int64_t latlon;

    latlon = *tptr & 0x3;
    latlon = (latlon << 32) | EXTRACT_32BITS(tptr+1);

    return latlon;
}

/*
 * Print private TIA extensions.
 */
static int
lldp_private_tia_print(const u_char *tptr, u_int tlv_len)
{
    int subtype, hexdump = FALSE;
    u_int8_t location_format;
    u_int16_t power_val;
    u_int8_t lci_len, ca_type, ca_len;

    subtype = *(tptr+3);

    printf("\n\t  %s Subtype (%u)",
           tok2str(lldp_tia_subtype_values, "unknown", subtype),
           subtype);

    switch (subtype) {
    case LLDP_PRIVATE_TIA_SUBTYPE_CAPABILITIES:
        printf("\n\t    Media capabilities [%s] (0x%04x)",
               bittok2str(lldp_tia_capabilities_values, "none",
                          EXTRACT_16BITS(tptr+4)), EXTRACT_16BITS(tptr+4));
        printf("\n\t    Device type [%s] (0x%02x)",
               tok2str(lldp_tia_device_type_values, "unknown", *(tptr+6)),
               *(tptr+6));
        break;

    case LLDP_PRIVATE_TIA_SUBTYPE_NETWORK_POLICY:
        printf("\n\t    Application type [%s] (0x%02x)",
               tok2str(lldp_tia_application_type_values, "none", *(tptr+4)),
               *(tptr+4));
        printf(", Flags [%s]", bittok2str(
                   lldp_tia_network_policy_bits_values, "none", *(tptr+5)));
        printf("\n\t    Vlan id %u",
               LLDP_EXTRACT_NETWORK_POLICY_VLAN(EXTRACT_16BITS(tptr+5)));
        printf(", L2 priority %u",
               LLDP_EXTRACT_NETWORK_POLICY_L2_PRIORITY(EXTRACT_16BITS(tptr+6)));
        printf(", DSCP value %u",
               LLDP_EXTRACT_NETWORK_POLICY_DSCP(EXTRACT_16BITS(tptr+6)));
        break;

    case LLDP_PRIVATE_TIA_SUBTYPE_LOCAL_ID:
        location_format = *(tptr+4);
        printf("\n\t    Location data format %s (0x%02x)",
               tok2str(lldp_tia_location_data_format_values, "unknown", location_format),
               location_format);

        switch (location_format) {
        case LLDP_TIA_LOCATION_DATA_FORMAT_COORDINATE_BASED:
            printf("\n\t    Latitude resolution %u, latitude value %" PRIu64,
                   (*(tptr+5)>>2), lldp_extract_latlon(tptr+5));
            printf("\n\t    Longitude resolution %u, longitude value %" PRIu64,
                   (*(tptr+10)>>2), lldp_extract_latlon(tptr+10));
            printf("\n\t    Altitude type %s (%u)",
                   tok2str(lldp_tia_location_altitude_type_values, "unknown",(*(tptr+15)>>4)),
                   (*(tptr+15)>>4));
            printf("\n\t    Altitude resolution %u, altitude value 0x%x",
                   (EXTRACT_16BITS(tptr+15)>>6)&0x3f,
                   ((EXTRACT_32BITS(tptr+16)&0x3fffffff)));
            printf("\n\t    Datum %s (0x%02x)",
                   tok2str(lldp_tia_location_datum_type_values, "unknown", *(tptr+20)),
                   *(tptr+20));
            break;

        case LLDP_TIA_LOCATION_DATA_FORMAT_CIVIC_ADDRESS:
            lci_len = *(tptr+5);
            printf("\n\t    LCI length %u, LCI what %s (0x%02x), Country-code ",
                   lci_len,
                   tok2str(lldp_tia_location_lci_what_values, "unknown", *(tptr+6)),
                   *(tptr+6));

            /* Country code */
            safeputs((const char *)(tptr+7), 2);

            lci_len = lci_len-3;
            tptr = tptr + 9;

            /* Decode each civic address element */	
            while (lci_len > 0) {
		ca_type = *(tptr);
                ca_len = *(tptr+1);

		tptr += 2;
                lci_len -= 2; 

                printf("\n\t      CA type \'%s\' (%u), length %u: ",
                       tok2str(lldp_tia_location_lci_catype_values, "unknown", ca_type),
                       ca_type, ca_len);

		/* basic sanity check */
		if ( ca_type == 0 || ca_len == 0) {
                    return hexdump;
		}

                safeputs((const char *)tptr, ca_len);
                tptr += ca_len;
                lci_len -= ca_len;
            }
            break;

        case LLDP_TIA_LOCATION_DATA_FORMAT_ECS_ELIN:
            printf("\n\t    ECS ELIN id ");
            safeputs((const char *)tptr+5, tlv_len-5);       
            break;

        default:
            printf("\n\t    Location ID ");
            print_unknown_data(tptr+5, "\n\t      ", tlv_len-5);
        }
        break;

    case LLDP_PRIVATE_TIA_SUBTYPE_EXTENDED_POWER_MDI:
        printf("\n\t    Power type [%s]",
               (*(tptr+4)&0xC0>>6) ? "PD device" : "PSE device");
        printf(", Power source [%s]",
               tok2str(lldp_tia_power_source_values, "none", (*(tptr+4)&0x30)>>4));
        printf("\n\t    Power priority [%s] (0x%02x)",
               tok2str(lldp_tia_power_priority_values, "none", *(tptr+4)&0x0f),
               *(tptr+4)&0x0f);
        power_val = EXTRACT_16BITS(tptr+5);
        if (power_val < LLDP_TIA_POWER_VAL_MAX) {
            printf(", Power %.1f Watts", ((float)power_val)/10);
        } else {
            printf(", Power %u (Reserved)", power_val);
        }
        break;

    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_HARDWARE_REV:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_FIRMWARE_REV:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SOFTWARE_REV:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_SERIAL_NUMBER:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MANUFACTURER_NAME:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_MODEL_NAME:
    case LLDP_PRIVATE_TIA_SUBTYPE_INVENTORY_ASSET_ID:
        printf("\n\t  %s ",
               tok2str(lldp_tia_inventory_values, "unknown", subtype));
        safeputs((const char *)tptr+4, tlv_len-4);
        break;

    default:
        hexdump = TRUE;
        break;
    }

    return hexdump;
}

static char *
lldp_network_addr_print(const u_char *tptr) {

    u_int8_t af;
    static char buf[BUFSIZE];
    const char * (*pfunc)(const u_char *);

    af = *tptr;
    switch (af) {
    case AFNUM_INET:
        pfunc = getname; 
        break;
#ifdef INET6
    case AFNUM_INET6:
        pfunc = getname6;
        break;
#endif
    case AFNUM_802:
        pfunc = etheraddr_string;
        break;
    default:
        pfunc = NULL;
        break;
    }

    if (!pfunc) {
        snprintf(buf, sizeof(buf), "AFI %s (%u), no AF printer !",
                 tok2str(af_values, "Unknown", af), af);
    } else {
        snprintf(buf, sizeof(buf), "AFI %s (%u): %s",
                 tok2str(af_values, "Unknown", af), af, (*pfunc)(tptr+1));
    }

    return buf;
}

static int
lldp_mgmt_addr_tlv_print(const u_char *pptr, u_int len) {

    u_int8_t mgmt_addr_len, intf_num_subtype, oid_len;
    const u_char *tptr;
    u_int tlen;
    
    tlen = len;
    tptr = pptr;

    mgmt_addr_len = *tptr++;
    tlen--;

    if (tlen < mgmt_addr_len) {
        return 0;
    }

    printf("\n\t  Management Address length %u, %s",
           mgmt_addr_len,
           lldp_network_addr_print(tptr));
    tptr += mgmt_addr_len;
    tlen -= mgmt_addr_len;

    if (tlen < LLDP_INTF_NUM_LEN) {
        return 0;
    }

    intf_num_subtype = *tptr;
    printf("\n\t  %s Interface Numbering (%u): %u",
           tok2str(lldp_intf_numb_subtype_values, "Unknown", intf_num_subtype),
           intf_num_subtype,
           EXTRACT_32BITS(tptr+1));

    tptr += LLDP_INTF_NUM_LEN;
    tlen -= LLDP_INTF_NUM_LEN;

    /*
     * The OID is optional.
     */
    if (tlen) {
        oid_len = *tptr;

        if (oid_len) {
            printf("\n\t  OID length %u", oid_len);
            safeputs((const char *)tptr+1, oid_len);
        }
    }

    return 1;
} 

void
lldp_print(register const u_char *pptr, register u_int len) {

    u_int8_t subtype;
    u_int16_t tlv, cap, ena_cap;
    u_int oui, tlen, hexdump, tlv_type, tlv_len;
    const u_char *tptr;
    
    tptr = pptr;
    tlen = len;

    if (vflag) {
        printf("LLDP, length %u", len);
    }

    while (tlen >= sizeof(tlv)) {

        TCHECK2(*tptr, sizeof(tlv));

        tlv = EXTRACT_16BITS(tptr);

        tlv_type = LLDP_EXTRACT_TYPE(tlv);
        tlv_len = LLDP_EXTRACT_LEN(tlv);
        hexdump = FALSE;

        tlen -= sizeof(tlv);
        tptr += sizeof(tlv);

        if (vflag) {
            printf("\n\t%s TLV (%u), length %u",
                   tok2str(lldp_tlv_values, "Unknown", tlv_type),
                   tlv_type, tlv_len);
        }

        /* infinite loop check */
        if (!tlv_type || !tlv_len) {
            break;
        }

        TCHECK2(*tptr, tlv_len);

        switch (tlv_type) {
        case LLDP_TTL_TLV:
            if (vflag) {
                printf(": TTL %us", EXTRACT_16BITS(tptr));
            }
            break;

        case LLDP_SYSTEM_NAME_TLV:

            /*
             * The system name is also print in non-verbose mode
             * similar to the CDP printer.
             */
            if (vflag) {
                printf(": ");
                safeputs((const char *)tptr, tlv_len);
            } else {
                printf("LLDP, name ");
                safeputs((const char *)tptr, tlv_len);
                printf(", length %u", len);
            }
            break;

        case LLDP_PORT_DESCR_TLV:
            if (vflag) {
                printf(": ");
                safeputs((const char *)tptr, tlv_len);
            }
            break;

        case LLDP_SYSTEM_DESCR_TLV:
            if (vflag) {
                printf("\n\t  ");
                safeputs((const char *)tptr, tlv_len);
            }
            break;


        case LLDP_CHASSIS_ID_TLV:
            if (vflag) {
                subtype = *tptr;
                printf("\n\t  Subtype %s (%u): ",
                       tok2str(lldp_chassis_subtype_values, "Unknown", subtype),
                       subtype);

                switch (subtype) {
                case LLDP_CHASSIS_MAC_ADDR_SUBTYPE:
                    printf("%s", etheraddr_string(tptr+1));
                    break;

                case LLDP_CHASSIS_INTF_NAME_SUBTYPE: /* fall through */
                case LLDP_CHASSIS_LOCAL_SUBTYPE:
                case LLDP_CHASSIS_CHASSIS_COMP_SUBTYPE:
                case LLDP_CHASSIS_INTF_ALIAS_SUBTYPE:
                case LLDP_CHASSIS_PORT_COMP_SUBTYPE:
                    safeputs((const char *)tptr+1, tlv_len-1);
                    break;

                case LLDP_CHASSIS_NETWORK_ADDR_SUBTYPE:
                    printf("%s", lldp_network_addr_print(tptr+1));
                    break;

                default:
                    hexdump = TRUE;
                    break;
                }
            }
            break;

        case LLDP_PORT_ID_TLV:
            if (vflag) {
                subtype = *tptr;
                printf("\n\t  Subtype %s (%u): ",
                       tok2str(lldp_port_subtype_values, "Unknown", subtype),
                       subtype);

                switch (subtype) {
                case LLDP_PORT_MAC_ADDR_SUBTYPE:
                    printf("%s", etheraddr_string(tptr+1));
                    break;

                case LLDP_PORT_INTF_NAME_SUBTYPE: /* fall through */
                case LLDP_PORT_LOCAL_SUBTYPE:
                case LLDP_PORT_AGENT_CIRC_ID_SUBTYPE:
                case LLDP_PORT_INTF_ALIAS_SUBTYPE:
                case LLDP_PORT_PORT_COMP_SUBTYPE:
                    safeputs((const char *)tptr+1, tlv_len-1);
                    break;

                case LLDP_PORT_NETWORK_ADDR_SUBTYPE:
                    printf("%s", lldp_network_addr_print(tptr+1));
                    break;

                default:
                    hexdump = TRUE;
                    break;
                }
            }
            break;

        case LLDP_PRIVATE_TLV:
            if (vflag) {
                oui = EXTRACT_24BITS(tptr);
                printf(": OUI %s (0x%06x)", tok2str(oui_values, "Unknown", oui), oui);
                
                switch (oui) {
                case OUI_IEEE_8021_PRIVATE:
                    hexdump = lldp_private_8021_print(tptr);
                    break;
                case OUI_IEEE_8023_PRIVATE:
                    hexdump = lldp_private_8023_print(tptr);
                    break;
                case OUI_TIA:
                    hexdump = lldp_private_tia_print(tptr, tlv_len);
                    break;
                default:
                    hexdump = TRUE;
                    break;
                }
            }
            break;

        case LLDP_SYSTEM_CAP_TLV:
            if (vflag) {
                cap = EXTRACT_16BITS(tptr);
                ena_cap = EXTRACT_16BITS(tptr+2);
                printf("\n\t  System  Capabilities [%s] (0x%04x)",
                       bittok2str(lldp_cap_values, "none", cap), cap);
                printf("\n\t  Enabled Capabilities [%s] (0x%04x)",
                       bittok2str(lldp_cap_values, "none", ena_cap), ena_cap);
            }
            break;

        case LLDP_MGMT_ADDR_TLV:
            if (vflag) {
                if (!lldp_mgmt_addr_tlv_print(tptr, tlen)) {
                    goto trunc;
                }
            }
            break;

        default:
            hexdump = TRUE;
            break;
        }

        /* do we also want to see a hex dump ? */
        if (vflag > 1 || (vflag && hexdump)) {
            print_unknown_data(tptr,"\n\t  ", tlv_len);
        }

        tlen -= tlv_len;
        tptr += tlv_len;
    }
    return;
 trunc:
    printf("\n\t[|LLDP]");
}

/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 4
 * End:
 */

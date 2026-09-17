/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/endpoints.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#define HALF "left"
#else
#define HALF "right"
#endif

LOG_MODULE_REGISTER(corne_diag, LOG_LEVEL_INF);

static const char *link_name(struct bt_conn *conn) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct bt_conn_info info;
    if (conn && bt_conn_get_info(conn, &info) == 0) {
        return info.role == BT_CONN_ROLE_CENTRAL ? "split" : "host";
    }
    return "unknown";
#else
    return "split";
#endif
}

static void connection_snapshot(struct bt_conn *conn, const char *event) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) != 0 || info.type != BT_CONN_TYPE_LE) {
        return;
    }
    char peer[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info.le.dst, peer, sizeof(peer));
    LOG_INF("half=%s link=%s conn=%u event=%s peer=%s security=%u interval_units=%u "
            "latency=%u timeout_units=%u", HALF, link_name(conn), bt_conn_index(conn),
            event, peer, bt_conn_get_security(conn), info.le.interval, info.le.latency,
            info.le.timeout);
#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
    if (info.le.phy) {
        LOG_INF("half=%s link=%s conn=%u phy_tx=%u phy_rx=%u", HALF, link_name(conn),
                bt_conn_index(conn), info.le.phy->tx_phy, info.le.phy->rx_phy);
    }
#endif
}

static void connected(struct bt_conn *conn, uint8_t err) {
    LOG_INF("half=%s link=%s conn=%u CONNECT err=0x%02x", HALF, link_name(conn),
            bt_conn_index(conn), err);
    if (!err) {
        connection_snapshot(conn, "connected");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    LOG_INF("half=%s link=%s conn=%u DISCONNECT reason=0x%02x", HALF, link_name(conn),
            bt_conn_index(conn), reason);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    LOG_INF("half=%s link=%s conn=%u SECURITY level=%u err=%d", HALF, link_name(conn),
            bt_conn_index(conn), level, err);
}

static void params_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                           uint16_t timeout) {
    connection_snapshot(conn, "params_updated");
}

#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
static void phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *info) {
    connection_snapshot(conn, "phy_updated");
}
#endif

BT_CONN_CB_DEFINE(corne_diagnostic_connections) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
    .le_param_updated = params_updated,
#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
    .le_phy_updated = phy_updated,
#endif
};

static void pairing_complete(struct bt_conn *conn, bool bonded) {
    LOG_INF("half=%s link=%s conn=%u PAIRING_COMPLETE bonded=%u", HALF, link_name(conn),
            bt_conn_index(conn), bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
    LOG_INF("half=%s link=%s conn=%u PAIRING_FAILED reason=%d", HALF, link_name(conn),
            bt_conn_index(conn), reason);
}

static struct bt_conn_auth_info_cb authentication = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

int __real_zmk_event_manager_raise(zmk_event_t *event);
int __wrap_zmk_event_manager_raise(zmk_event_t *event) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(event);
    if (pos) {
        LOG_INF("half=%s POSITION source=%u position=%u state=%u event_ms=%lld now_ms=%lld",
                HALF, pos->source, pos->position, pos->state, pos->timestamp, k_uptime_get());
    }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const struct zmk_keycode_state_changed *key = as_zmk_keycode_state_changed(event);
    if (key) {
        LOG_INF("half=%s KEYCODE page=%u usage=%u state=%u event_ms=%lld now_ms=%lld",
                HALF, key->usage_page, key->keycode, key->state, key->timestamp, k_uptime_get());
    }
#endif
    return __real_zmk_event_manager_raise(event);
}

/* Zephyr's inline bt_gatt_notify() also reaches this external function.
 * Keep parameters, completion callback, data and return value unchanged.
 * rc=0 proves acceptance by the stack, never delivery to the host OS. */
static atomic_t notify_sequence;
int __real_bt_gatt_notify_cb(struct bt_conn *conn, struct bt_gatt_notify_params *params);
int __wrap_bt_gatt_notify_cb(struct bt_conn *conn, struct bt_gatt_notify_params *params) {
    unsigned int seq = (unsigned int)atomic_inc(&notify_sequence);
    LOG_INF("half=%s NOTIFY_BEGIN seq=%u conn=%d attr=%u len=%u", HALF, seq,
            conn ? bt_conn_index(conn) : -1, params->attr ? params->attr->handle : 0,
            params->len);
    LOG_HEXDUMP_INF(params->data, params->len, HALF " NOTIFY payload");
    int rc = __real_bt_gatt_notify_cb(conn, params);
    LOG_INF("half=%s NOTIFY_END seq=%u rc=%d", HALF, seq, rc);
    return rc;
}

extern struct k_msgq physical_layouts_kscan_msgq;
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
extern struct k_msgq peripheral_event_msgq;
extern struct k_msgq zmk_hog_keyboard_msgq;
#else
extern struct k_msgq position_state_msgq;
#endif

int __real_z_impl_k_msgq_put(struct k_msgq *msgq, const void *data, k_timeout_t timeout);
int __wrap_z_impl_k_msgq_put(struct k_msgq *msgq, const void *data, k_timeout_t timeout) {
    int rc = __real_z_impl_k_msgq_put(msgq, data, timeout);
    if (rc) {
        const char *name = NULL;
        if (msgq == &physical_layouts_kscan_msgq) {
            name = "matrix";
        }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        else if (msgq == &peripheral_event_msgq) {
            name = "split_rx";
        } else if (msgq == &zmk_hog_keyboard_msgq) {
            name = "hid_tx";
        }
#else
        else if (msgq == &position_state_msgq) {
            name = "split_tx";
        }
#endif
        if (name) {
            LOG_ERR("half=%s QUEUE_PUT_FAILED queue=%s rc=%d", HALF, name, rc);
        }
    }
    return rc;
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
int __real_zmk_hog_send_keyboard_report(struct zmk_hid_keyboard_report_body *report);
int __wrap_zmk_hog_send_keyboard_report(struct zmk_hid_keyboard_report_body *report) {
    LOG_HEXDUMP_INF(report, sizeof(*report), "left HID_ENQUEUE body");
    int rc = __real_zmk_hog_send_keyboard_report(report);
    LOG_INF("half=left HID_ENQUEUE rc=%d", rc);
    return rc;
}

static int output_command(const struct shell *sh, size_t argc, char **argv) {
    enum zmk_transport transport;
    if (!strcmp(argv[1], "ble")) {
        transport = ZMK_TRANSPORT_BLE;
    } else if (!strcmp(argv[1], "usb")) {
        transport = ZMK_TRANSPORT_USB;
    } else {
        shell_error(sh, "Use: corne output ble|usb");
        return -EINVAL;
    }
    int rc = zmk_endpoint_set_preferred_transport(transport);
    shell_print(sh, "Preferred transport=%u rc=%d (saved setting; restore with corne output usb)",
                transport, rc);
    return rc;
}
#endif

static void snapshot_each(struct bt_conn *conn, void *data) {
    connection_snapshot(conn, "status");
}

static int status_command(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "half=%s uptime_ms=%lld; all upstream ADC/matrix logs on this port are local",
                HALF, k_uptime_get());
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    shell_print(sh, "Selected endpoint=%u (0=none 1=USB 2=BLE)",
                zmk_endpoint_get_selected().transport);
#endif
    bt_conn_foreach(BT_CONN_TYPE_LE, snapshot_each, NULL);
#if DT_HAS_CHOSEN(zmk_battery)
    const struct device *battery = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
    struct sensor_value voltage, soc;
    if (device_is_ready(battery) &&
        sensor_channel_get(battery, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage) == 0 &&
        sensor_channel_get(battery, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &soc) == 0) {
        shell_print(sh, "half=%s BATTERY cached_mv=%d cached_soc=%d (last upstream sample)",
                    HALF, voltage.val1 * 1000 + voltage.val2 / 1000, soc.val1);
    }
#endif
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(corne_commands,
    SHELL_CMD(status, NULL, "Connection, endpoint and cached local battery snapshot", status_command),
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    SHELL_CMD_ARG(output, NULL, "ble|usb: select and save preferred HID transport", output_command, 2, 0),
#endif
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(corne, &corne_commands, "Corne diagnostics", NULL);

static int diagnostics_init(void) {
    LOG_INF("half=%s diagnostic observers enabled; clocks are local to each half", HALF);
    return bt_conn_auth_info_cb_register(&authentication);
}

SYS_INIT(diagnostics_init, APPLICATION, 99);

/* SPDX-License-Identifier: MIT
 * Legacy advertising transport for the pinned Prospector payload builder.
 * ZMK owns connection advertising when possible; our fallback uses the same
 * HID identity and one-time lifetime. Never replace pairing AD with name-only AD.
 */
static struct bt_data zmk_ad_restore[] = {
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, BT_BYTES_LIST_LE16(BT_APPEARANCE_HID_KEYBOARD)),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_SOME, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};
static struct bt_data piggyback_sd[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t *)&manufacturer_data, sizeof(manufacturer_data)),
};
static struct bt_data prospector_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t *)&manufacturer_data, sizeof(manufacturer_data)),
};
/* Flags (3) + name header (2) + 26 name bytes <= 31. */
#define NAME_ADV_INTERVAL_ACTIVE 25
#define NAME_ADV_INTERVAL_IDLE 2
static uint32_t adv_cycle_counter;
static char name_adv_buffer[27];
static struct bt_data name_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, name_adv_buffer, 0),
};
/* Pairing AD: appearance (4) + flags (3) + UUIDs (6) + name (2+16).
 * No USE_NAME: explicit name placement works with enum options on all versions.
 * The scan response is only the unchanged 28-byte manufacturer AD structure.
 */
static struct bt_data proxy_ad[] = {
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, BT_BYTES_LIST_LE16(BT_APPEARANCE_HID_KEYBOARD)),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_SOME, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
    BT_DATA(BT_DATA_NAME_COMPLETE, name_adv_buffer, 0),
};
static const struct bt_le_adv_param prospector_adv_params = {
#if SATELLITE_HAS_SCANNABLE
    .options = BT_LE_ADV_OPT_SCANNABLE,
#endif
    .interval_min = ZMK_SATELLITE_ADV_MIN,
    .interval_max = ZMK_SATELLITE_ADV_MAX,
};
static const struct bt_le_adv_param proxy_connectable_params = {
    .options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
};
static const struct bt_le_adv_param burst_adv_params;

static int satellite_stop_own(void)
{
    if (!prospector_adv_active) return 0;
    int err = bt_le_adv_stop();
    if (err) {
        LOG_WRN("Satellite: advertising stop failed: %d", err);
        return err; /* Keep ownership so the next update can retry safely. */
    }
    prospector_adv_active = false;
    prospector_adv_connectable = false;
    zmk_adv_was_active = false;
    return 0;
}

static int satellite_advertise(bool split_ready)
{
    bool connected = zmk_ble_active_profile_is_connected();
    bool connectable = split_ready && !connected;
    int err;
    proxy_ad[3].data_len = MIN(name_ad[1].data_len, 16);
    proxy_ad[3].type = name_ad[1].data_len > 16 ? BT_DATA_NAME_SHORTENED : BT_DATA_NAME_COMPLETE;

    if (prospector_adv_active && prospector_adv_connectable != connectable) {
        err = satellite_stop_own();
        if (err) return err;
    }
    if (prospector_adv_active) {
        adv_cycle_counter++;
        uint32_t interval = is_active ? NAME_ADV_INTERVAL_ACTIVE : NAME_ADV_INTERVAL_IDLE;
        bool send_name = atomic_get(&burst_remaining) == 0 &&
                         adv_cycle_counter % interval == 0 && name_ad[1].data_len > 0;
        if (prospector_adv_connectable) {
            err = bt_le_adv_update_data(proxy_ad, ARRAY_SIZE(proxy_ad),
                                        piggyback_sd, ARRAY_SIZE(piggyback_sd));
        } else if (send_name) {
            err = bt_le_adv_update_data(name_ad, ARRAY_SIZE(name_ad), NULL, 0);
        } else {
            err = bt_le_adv_update_data(prospector_ad, ARRAY_SIZE(prospector_ad),
                                        &name_ad[1], name_ad[1].data_len ? 1 : 0);
        }
        if (!err) return 0;
        if (err != -EAGAIN && err != -EINVAL) return err;
        /* Legacy stop/ONE_TIME may remove the advertiser, yielding -EINVAL,
         * not -EAGAIN. Release any remaining owned set before reacquiring it. */
        err = satellite_stop_own();
        if (err) return err;
    }

    /* ZMK's legacy options decide automatic name placement, not #ifdef(enum).
     * FORCE_NAME_IN_AD: 13-byte HID AD + auto-name; manufacturer in SD (28).
     * Older API: manufacturer in AD (31); HID identity + auto-name in SD.
     */
#if SATELLITE_HAS_FORCE_NAME_IN_AD
    err = bt_le_adv_update_data(zmk_ad_restore, ARRAY_SIZE(zmk_ad_restore),
                                piggyback_sd, ARRAY_SIZE(piggyback_sd));
#else
    struct bt_data legacy_sd[] = {zmk_ad_restore[0], zmk_ad_restore[2]};
    err = bt_le_adv_update_data(prospector_ad, ARRAY_SIZE(prospector_ad),
                                legacy_sd, ARRAY_SIZE(legacy_sd));
#endif
    if (!err) {
        zmk_adv_was_active = true;
        return 0;
    }
    zmk_adv_was_active = false;
    if (err != -EAGAIN && err != -EINVAL) return err;
    const struct bt_le_adv_param *params = !split_ready ? &burst_adv_params :
        connected ? &prospector_adv_params : &proxy_connectable_params;
    if (connectable) {
        err = bt_le_adv_start(params, proxy_ad, ARRAY_SIZE(proxy_ad),
                              piggyback_sd, ARRAY_SIZE(piggyback_sd));
    } else {
        err = bt_le_adv_start(params, prospector_ad, ARRAY_SIZE(prospector_ad),
                              split_ready ? &name_ad[1] : NULL,
                              split_ready && name_ad[1].data_len ? 1 : 0);
    }
    if (!err) {
        prospector_adv_active = true;
        prospector_adv_connectable = connectable;
    }
    return err; /* Includes -EALREADY races: retry without stopping ZMK's set. */
}

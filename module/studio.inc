/* SPDX-License-Identifier: MIT
 * Studio owns the entire legacy advertising set while BLE unlock is active.
 * In supported ZMK, unlock attempts advertising before raising its event, so
 * merely stopping our advertiser in the listener is insufficient: retry ZMK's
 * existing API after releasing the set. Never reconstruct Studio's AD/SD here.
 */
#if SATELLITE_STUDIO_BLE
static atomic_t studio_advertising_ready = ATOMIC_INIT(0);
#endif

static bool satellite_studio_unlocked(void)
{
#if SATELLITE_STUDIO_BLE
    return zmk_studio_core_get_lock_state() == ZMK_STUDIO_CORE_LOCK_STATE_UNLOCKED;
#else
    return false;
#endif
}

static bool satellite_yield_to_studio(void)
{
#if SATELLITE_STUDIO_BLE
    if (!satellite_studio_unlocked()) {
        atomic_set(&studio_advertising_ready, 0);
        return false;
    }

    int err = satellite_stop_own();
    zmk_adv_was_active = false;
    atomic_set(&burst_remaining, 0);
    if (!err && !atomic_get(&studio_advertising_ready)) {
        err = zmk_ble_set_directed_advertising(true);
        atomic_set(&studio_advertising_ready, !err);
    }
    if (err) {
        LOG_WRN("Satellite: Studio advertising handoff retry: %d", err);
        k_work_reschedule(&adv_work, K_MSEC(250));
    }
    return true;
#else
    return false;
#endif
}

#if SATELLITE_STUDIO_BLE
static int satellite_studio_listener(const zmk_event_t *eh)
{
    if (!as_zmk_studio_core_lock_state_changed(eh)) return ZMK_EV_EVENT_BUBBLE;
    atomic_set(&studio_advertising_ready, 0);
    if (adv_started) k_work_reschedule(&adv_work, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(satellite_studio, satellite_studio_listener);
ZMK_SUBSCRIPTION(satellite_studio, zmk_studio_core_lock_state_changed);
#endif

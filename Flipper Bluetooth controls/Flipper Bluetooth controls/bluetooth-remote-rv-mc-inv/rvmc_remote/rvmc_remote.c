/*
 * RV/MC/INV Remote (BLE) for Flipper Zero
 * ----------------------------------------
 * A Bluetooth LE HID keyboard remote that types short capitalized text
 * shortcuts instead of acting like a presentation clicker:
 *
 *      Up button   ->  types "RV"
 *      Down button ->  types "MC"
 *      OK button   ->  types "INV"
 *      Hold Back   ->  exit this app
 *
 * Each letter is sent as Shift + <letter>, so the host receives the
 * capitalized text regardless of Caps Lock state.
 *
 * Pair your Flipper from the host computer's Bluetooth settings, put the
 * cursor in a text field, and press away.
 */

#include <furi.h>
#include <furi_hal_bt.h>
#include <furi_hal_random.h>

#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define TAG "RvMcInvRemote"

/*
 * Standard USB HID keyboard "usage IDs" for the letters we type. These are
 * part of the USB HID spec and never change, so defining them here keeps the
 * app independent of any particular firmware header layout. The Shift
 * modifier lives in the high byte of the 16-bit value passed to
 * ble_profile_hid_kb_press(), matching the firmware's KEY_MOD_LEFT_SHIFT.
 */
#define KEY_C 0x06
#define KEY_I 0x0C
#define KEY_M 0x10
#define KEY_N 0x11
#define KEY_R 0x15
#define KEY_V 0x19

#define MOD_LEFT_SHIFT (1 << 9)

/*
 * Keep the HID pairing keys in the app's own storage folder so we don't
 * disturb the Flipper's default (serial) Bluetooth pairing.
 */
#define HID_KEYS_DIR EXT_PATH("apps_data/rvmc_remote")
#define HID_KEYS_PATH EXT_PATH("apps_data/rvmc_remote/.bt_hid.keys")

/*
 * The bt service stores BLE bonds together with the device's "root security
 * keys" (ERK/IRK) in each keys file; hosts use the IRK to recognise a
 * device's identity independently of its MAC address. When this app's keys
 * file does not exist yet, bt_keys_storage_load() fails silently and the
 * service keeps whatever keys are already in RAM -- the ones from the
 * Flipper's DEFAULT file (serial BT / stock "Control <name>" remote). The
 * first pairing then bakes that default identity into this app's file, and a
 * host bonded to both devices sees two entries with the same identity keys:
 * it keeps mixing the bonds up (drops, failed re-pairing) until one of the
 * pairings is deleted.
 *
 * These constants mirror bt_keys_storage.c (saved_struct file layout:
 * GapRootSecurityKeys followed by the raw radio bond database).
 */
#define BT_DEFAULT_KEYS_PATH INT_PATH(".bt.keys")
#define BT_KEYS_FILE_MAGIC (0x18)
#define BT_KEYS_FILE_VERSION (1)

/* Identity used by all profiles on firmwares before the per-file keys format
 * (bt_keys_storage.c's gap_legacy_irk/gap_legacy_erk). */
static const GapRootSecurityKeys legacy_root_keys = {
    .erk = {0xfe, 0xdc, 0xba, 0x09, 0x87, 0x65, 0x43, 0x21,
            0xfe, 0xdc, 0xba, 0x09, 0x87, 0x65, 0x43, 0x21},
    .irk = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
            0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
};

/*
 * Advertise as our OWN Bluetooth device, separate from the Flipper's serial
 * Bluetooth, the stock Bluetooth Remote, and the Enter Remote app. A non-zero
 * mac_xor gives this app a unique BLE MAC address, so the host sees a
 * brand-new device to pair with instead of colliding with an existing bond --
 * a collision shows up as the host rapidly connecting and disconnecting
 * because its saved pairing keys don't match this app's. The prefix
 * (< 8 chars) makes the app easy to spot in the host's Bluetooth list.
 */
static const BleProfileHidParams hid_params = {
    .device_name_prefix = "RVMC",
    .mac_xor = 0x00E2,
};

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    Gui* gui;
    ViewPort* view_port;
    Bt* bt;
    NotificationApp* notifications;
    FuriHalBleProfileBase* hid_profile;
    bool connected;
} RvMcInvRemote;

/* ------------------------------------------------------------------ */
/* GUI                                                                */
/* ------------------------------------------------------------------ */

static void rvmc_remote_draw_callback(Canvas* canvas, void* ctx) {
    RvMcInvRemote* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool connected = app->connected;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "RV/MC/INV Remote");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, connected ? "Connected" : "Pair \"RVMC...\" device");

    canvas_draw_line(canvas, 0, 26, 128, 26);

    canvas_draw_str(canvas, 2, 37, "Up = RV");
    canvas_draw_str(canvas, 2, 47, "Down = MC");
    canvas_draw_str(canvas, 2, 57, "OK = INV");
    canvas_draw_str(canvas, 66, 64, "Hold Back: exit");
}

static void rvmc_remote_input_callback(InputEvent* event, void* ctx) {
    RvMcInvRemote* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* ------------------------------------------------------------------ */
/* Input handling                                                     */
/* ------------------------------------------------------------------ */

/* Type a sequence of letters as capital letters (Shift held per key). */
static void rvmc_remote_type_keys(RvMcInvRemote* app, const uint16_t* keys, size_t count) {
    for(size_t i = 0; i < count; i++) {
        ble_profile_hid_kb_press(app->hid_profile, MOD_LEFT_SHIFT | keys[i]);
        furi_delay_ms(12);
        ble_profile_hid_kb_release(app->hid_profile, MOD_LEFT_SHIFT | keys[i]);
        furi_delay_ms(12);
    }
}

/* Returns false when the app should exit. */
static bool rvmc_remote_handle_input(RvMcInvRemote* app, InputEvent* event) {
    static const uint16_t rv_keys[] = {KEY_R, KEY_V};
    static const uint16_t mc_keys[] = {KEY_M, KEY_C};
    static const uint16_t inv_keys[] = {KEY_I, KEY_N, KEY_V};

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        return false; /* hold Back -> quit the app */
    }

    /* One text shortcut per click; ignore holds/repeats on the other keys. */
    if(event->type != InputTypeShort) return true;

    switch(event->key) {
    case InputKeyUp:
        rvmc_remote_type_keys(app, rv_keys, COUNT_OF(rv_keys));
        break;
    case InputKeyDown:
        rvmc_remote_type_keys(app, mc_keys, COUNT_OF(mc_keys));
        break;
    case InputKeyOk:
        rvmc_remote_type_keys(app, inv_keys, COUNT_OF(inv_keys));
        break;
    default:
        break;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Bluetooth                                                          */
/* ------------------------------------------------------------------ */

static bool rvmc_remote_read_root_keys(const char* path, GapRootSecurityKeys* keys) {
    uint8_t magic;
    uint8_t version;
    size_t size;
    if(!saved_struct_get_metadata(path, &magic, &version, &size)) return false;
    if(magic != BT_KEYS_FILE_MAGIC || version != BT_KEYS_FILE_VERSION) return false;
    if(size < sizeof(GapRootSecurityKeys)) return false;

    uint8_t* data = malloc(size);
    bool loaded = saved_struct_load(path, data, size, magic, version);
    if(loaded) memcpy(keys, data, sizeof(GapRootSecurityKeys));
    free(data);
    return loaded;
}

/*
 * Make sure our keys file exists and carries its OWN root security keys
 * before the bt service loads it. If it is missing, unreadable, or shares
 * its identity with the Flipper's default Bluetooth (see the comment at
 * BT_DEFAULT_KEYS_PATH), replace it: fresh random root keys plus an all-zero
 * bond area. The zeroed bond area doubles as a wipe of the default profile's
 * stale bonds from the radio's key RAM when the file is loaded. Existing
 * pairings of THIS app survive only when the file was already healthy.
 */
static void rvmc_remote_ensure_own_identity(void) {
    GapRootSecurityKeys own_keys;
    bool keys_ok = rvmc_remote_read_root_keys(HID_KEYS_PATH, &own_keys);

    if(keys_ok) {
        GapRootSecurityKeys default_keys;
        if(rvmc_remote_read_root_keys(BT_DEFAULT_KEYS_PATH, &default_keys) &&
           memcmp(&own_keys, &default_keys, sizeof(own_keys)) == 0) {
            FURI_LOG_W(TAG, "Keys file shares the default BT identity, regenerating");
            keys_ok = false;
        }
        if(keys_ok && memcmp(&own_keys, &legacy_root_keys, sizeof(own_keys)) == 0) {
            FURI_LOG_W(TAG, "Keys file uses the legacy shared identity, regenerating");
            keys_ok = false;
        }
    }
    if(keys_ok) return;

    uint8_t* nvm_buff;
    uint16_t nvm_size;
    furi_hal_bt_get_key_storage_buff(&nvm_buff, &nvm_size);

    size_t file_size = sizeof(GapRootSecurityKeys) + nvm_size;
    uint8_t* data = malloc(file_size);
    memset(data, 0, file_size);
    furi_hal_random_fill_buf(data, sizeof(GapRootSecurityKeys));

    if(!saved_struct_save(HID_KEYS_PATH, data, file_size, BT_KEYS_FILE_MAGIC, BT_KEYS_FILE_VERSION)) {
        FURI_LOG_E(TAG, "Failed to write keys file");
    }
    free(data);
}

static void rvmc_remote_bt_status_callback(BtStatus status, void* context) {
    RvMcInvRemote* app = context;
    bool connected = (status == BtStatusConnected);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->connected = connected;
    furi_mutex_release(app->mutex);

    /* Blue LED on when a host is connected, like the stock remote. */
    notification_message(
        app->notifications, connected ? &sequence_set_blue_255 : &sequence_reset_blue);
    view_port_update(app->view_port);
}

static void rvmc_remote_bt_start(RvMcInvRemote* app) {
    /* Ensure the HID keys folder exists (harmless if it already does). */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, HID_KEYS_DIR);
    furi_record_close(RECORD_STORAGE);

    rvmc_remote_ensure_own_identity();

    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(app->bt, HID_KEYS_PATH);

    app->hid_profile = bt_profile_start(app->bt, ble_profile_hid, (void*)&hid_params);
    furi_check(app->hid_profile);

    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(app->bt, rvmc_remote_bt_status_callback, app);
}

static void rvmc_remote_bt_stop(RvMcInvRemote* app) {
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    notification_message(app->notifications, &sequence_reset_blue);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    /* Restore the Flipper's normal Bluetooth so serial/other apps work again. */
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static RvMcInvRemote* rvmc_remote_alloc(void) {
    RvMcInvRemote* app = malloc(sizeof(RvMcInvRemote));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->connected = false;
    app->hid_profile = NULL;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, rvmc_remote_draw_callback, app);
    view_port_input_callback_set(app->view_port, rvmc_remote_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->bt = furi_record_open(RECORD_BT);

    return app;
}

static void rvmc_remote_free(RvMcInvRemote* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t rvmc_remote_app(void* p) {
    UNUSED(p);
    RvMcInvRemote* app = rvmc_remote_alloc();
    rvmc_remote_bt_start(app);

    InputEvent event;
    for(bool running = true; running;) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(!rvmc_remote_handle_input(app, &event)) {
                running = false;
            }
        }
    }

    ble_profile_hid_kb_release_all(app->hid_profile);
    rvmc_remote_bt_stop(app);
    rvmc_remote_free(app);
    return 0;
}

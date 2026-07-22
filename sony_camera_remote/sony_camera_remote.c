/*
 * Sony Camera Remote (BLE) for Flipper Zero
 * ------------------------------------------
 * A Bluetooth LE remote for Sony cameras.
 *
 *
 *      Up          ->  Shutter
 *      Ok          ->  Record Start/Stop
 *      Hold Back   ->  exit this app
 *
 */

#include <furi.h>
#include <furi_hal_bt.h>

#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <bt/bt_service/bt.h>

#define TAG "SonyCameraRemote"

// TODO: Find the correct UUIDs for Sony Camera Remote Service
#define SONY_CAMERA_REMOTE_SERVICE_UUID 0x0000
#define SONY_CAMERA_REMOTE_CHARACTERISTIC_UUID 0x0000


typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    Gui* gui;
    ViewPort* view_port;
    Bt* bt;
    NotificationApp* notifications;
    bool connected;
} SonyCameraRemote;

/* ------------------------------------------------------------------ */
/* GUI                                                                */
/* ------------------------------------------------------------------ */

static void sony_camera_remote_draw_callback(Canvas* canvas, void* ctx) {
    SonyCameraRemote* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool connected = app->connected;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Sony Camera Remote");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, connected ? "Connected" : "Scanning...");

    canvas_draw_line(canvas, 0, 26, 128, 26);

    canvas_draw_str(canvas, 2, 37, "Up = Shutter");
    canvas_draw_str(canvas, 2, 47, "Ok = Record");
    canvas_draw_str(canvas, 2, 57, "Hold Back to exit");
}

static void sony_camera_remote_input_callback(InputEvent* event, void* ctx) {
    SonyCameraRemote* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* ------------------------------------------------------------------ */
/* Input handling                                                     */
/* ------------------------------------------------------------------ */

/* Returns false when the app should exit. */
static bool sony_camera_remote_handle_input(SonyCameraRemote* app, InputEvent* event) {
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        return false; /* hold Back -> quit the app */
    }

    if (event->type == InputTypeShort) {
        switch (event->key) {
        case InputKeyUp:
            // TODO: Send shutter command
            break;
        case InputKeyOk:
            // TODO: Send record command
            break;
        default:
            break;
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Bluetooth                                                          */
/* ------------------------------------------------------------------ */

// TODO: Implement Bluetooth connection and command sending

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static SonyCameraRemote* sony_camera_remote_alloc(void) {
    SonyCameraRemote* app = malloc(sizeof(SonyCameraRemote));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->connected = false;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, sony_camera_remote_draw_callback, app);
    view_port_input_callback_set(app->view_port, sony_camera_remote_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->bt = furi_record_open(RECORD_BT);

    return app;
}

static void sony_camera_remote_free(SonyCameraRemote* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t sony_camera_remote_app(void* p) {
    UNUSED(p);
    SonyCameraRemote* app = sony_camera_remote_alloc();

    // TODO: Start Bluetooth
    InputEvent event;
    for(bool running = true; running;) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(!sony_camera_remote_handle_input(app, &event)) {
                running = false;
            }
        }
    }

    // TODO: Stop Bluetooth
    sony_camera_remote_free(app);
    return 0;
}

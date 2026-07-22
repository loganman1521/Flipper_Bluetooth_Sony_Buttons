#include "sony_transport.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input_queue;
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notifications;
    SonyTransport* transport;
    SonyTransportState state;
    bool recording;
} SonyA7IvRemote;

static const char* sony_a7_iv_remote_state_text(SonyTransportState state) {
    switch(state) {
    case SonyTransportScanning:
        return "Scanning for camera";
    case SonyTransportConnecting:
        return "Connecting";
    case SonyTransportPairing:
        return "Confirm pairing on A7 IV";
    case SonyTransportDiscovering:
        return "Finding remote service";
    case SonyTransportReady:
        return "Connected";
    case SonyTransportDisconnected:
        return "Connection lost";
    case SonyTransportError:
        return "Bluetooth error";
    case SonyTransportUnavailable:
    default:
        return "Central firmware required";
    }
}

static void sony_a7_iv_remote_draw_callback(Canvas* canvas, void* context) {
    SonyA7IvRemote* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const SonyTransportState state = app->state;
    const bool recording = app->recording;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Sony A7 IV Remote");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, sony_a7_iv_remote_state_text(state));
    canvas_draw_line(canvas, 0, 26, 128, 26);
    canvas_draw_str(canvas, 2, 39, "Up = shutter");
    canvas_draw_str(canvas, 2, 50, recording ? "OK = stop recording" : "OK = start recording");
    canvas_draw_str(canvas, 2, 62, "Hold Back = exit");
}

static void sony_a7_iv_remote_input_callback(InputEvent* event, void* context) {
    SonyA7IvRemote* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static void sony_a7_iv_remote_state_callback(SonyTransportState state, void* context) {
    SonyA7IvRemote* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = state;
    furi_mutex_release(app->mutex);
    notification_message(
        app->notifications,
        state == SonyTransportReady ? &sequence_set_blue_255 : &sequence_reset_blue);
    view_port_update(app->view_port);
}

static void sony_a7_iv_remote_status_callback(SonyCameraStatus status, void* context) {
    SonyA7IvRemote* app = context;
    if(status.type == SonyCameraStatusRecording) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->recording = status.active;
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }
}

static bool sony_a7_iv_remote_send_shutter(SonyA7IvRemote* app) {
    const SonyRemoteButton sequence[] = {
        SonyRemoteButtonShutterHalfDown,
        SonyRemoteButtonShutterFullDown,
        SonyRemoteButtonShutterHalfUp,
        SonyRemoteButtonShutterFullUp,
    };

    for(size_t i = 0; i < COUNT_OF(sequence); ++i) {
        if(!sony_transport_send_button(app->transport, sequence[i])) return false;
        furi_delay_ms(12);
    }
    return true;
}

static bool sony_a7_iv_remote_send_record(SonyA7IvRemote* app) {
    if(!sony_transport_send_button(app->transport, SonyRemoteButtonRecordDown)) return false;
    furi_delay_ms(12);
    return sony_transport_send_button(app->transport, SonyRemoteButtonRecordUp);
}

static bool sony_a7_iv_remote_handle_input(SonyA7IvRemote* app, const InputEvent* event) {
    if(event->key == InputKeyBack && event->type == InputTypeLong) return false;
    if(event->type != InputTypeShort) return true;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const bool ready = app->state == SonyTransportReady;
    furi_mutex_release(app->mutex);
    if(!ready) return true;

    bool sent = true;
    if(event->key == InputKeyUp) {
        sent = sony_a7_iv_remote_send_shutter(app);
    } else if(event->key == InputKeyOk) {
        sent = sony_a7_iv_remote_send_record(app);
    }

    if(!sent) sony_a7_iv_remote_state_callback(SonyTransportError, app);
    return true;
}

static SonyA7IvRemote* sony_a7_iv_remote_alloc(void) {
    SonyA7IvRemote* app = malloc(sizeof(SonyA7IvRemote));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->state = SonyTransportScanning;
    app->recording = false;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, sony_a7_iv_remote_draw_callback, app);
    view_port_input_callback_set(app->view_port, sony_a7_iv_remote_input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->transport = sony_transport_alloc(
        sony_a7_iv_remote_state_callback, sony_a7_iv_remote_status_callback, app);
    return app;
}

static void sony_a7_iv_remote_free(SonyA7IvRemote* app) {
    sony_transport_free(app->transport);
    notification_message(app->notifications, &sequence_reset_blue);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t sony_a7_iv_remote_app(void* context) {
    UNUSED(context);
    SonyA7IvRemote* app = sony_a7_iv_remote_alloc();
    sony_transport_start(app->transport);

    InputEvent event;
    for(bool running = true; running;) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            running = sony_a7_iv_remote_handle_input(app, &event);
        }
    }

    sony_transport_stop(app->transport);
    sony_a7_iv_remote_free(app);
    return 0;
}

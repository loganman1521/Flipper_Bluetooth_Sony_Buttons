#include "sony_transport.h"

#include <furi.h>

#ifndef SONY_BLE_CENTRAL_INTERNAL

/*
 * Flipper firmware currently exposes BLE peripheral/profile APIs to FAPs but
 * not the central/GATT-client operations required by a Sony camera. This
 * transport keeps that limitation explicit. A firmware-side central adapter
 * will replace this file without changing the UI or Sony protocol layer.
 */
struct SonyTransport {
    SonyTransportStateCallback state_callback;
    SonyTransportStatusCallback status_callback;
    void* context;
};

SonyTransport* sony_transport_alloc(
    SonyTransportStateCallback state_callback,
    SonyTransportStatusCallback status_callback,
    void* context) {
    SonyTransport* transport = malloc(sizeof(SonyTransport));
    transport->state_callback = state_callback;
    transport->status_callback = status_callback;
    transport->context = context;
    return transport;
}

void sony_transport_free(SonyTransport* transport) {
    free(transport);
}

bool sony_transport_start(SonyTransport* transport) {
    transport->state_callback(SonyTransportUnavailable, transport->context);
    return false;
}

void sony_transport_stop(SonyTransport* transport) {
    UNUSED(transport);
}

bool sony_transport_send_button(SonyTransport* transport, SonyRemoteButton button) {
    UNUSED(transport);
    UNUSED(button);
    return false;
}

#endif /* SONY_BLE_CENTRAL_INTERNAL */

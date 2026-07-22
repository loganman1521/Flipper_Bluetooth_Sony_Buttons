#pragma once

#include "sony_protocol.h"

#include <stdbool.h>

typedef enum {
    SonyTransportUnavailable,
    SonyTransportScanning,
    SonyTransportConnecting,
    SonyTransportPairing,
    SonyTransportDiscovering,
    SonyTransportReady,
    SonyTransportDisconnected,
    SonyTransportError,
} SonyTransportState;

typedef void (*SonyTransportStateCallback)(SonyTransportState state, void* context);
typedef void (*SonyTransportStatusCallback)(SonyCameraStatus status, void* context);

typedef struct SonyTransport SonyTransport;

SonyTransport* sony_transport_alloc(
    SonyTransportStateCallback state_callback,
    SonyTransportStatusCallback status_callback,
    void* context);
void sony_transport_free(SonyTransport* transport);

bool sony_transport_start(SonyTransport* transport);
void sony_transport_stop(SonyTransport* transport);
bool sony_transport_send_button(SonyTransport* transport, SonyRemoteButton button);

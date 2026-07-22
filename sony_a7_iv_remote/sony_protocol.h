#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Sony Alpha BLE Remote Control service. */
extern const uint8_t sony_remote_service_uuid[16];

typedef enum {
    SonyRemoteButtonShutterHalfUp = 0x06,
    SonyRemoteButtonShutterHalfDown = 0x07,
    SonyRemoteButtonShutterFullUp = 0x08,
    SonyRemoteButtonShutterFullDown = 0x09,
    SonyRemoteButtonRecordUp = 0x0E,
    SonyRemoteButtonRecordDown = 0x0F,
    SonyRemoteButtonAfOnUp = 0x14,
    SonyRemoteButtonAfOnDown = 0x15,
    SonyRemoteButtonC1Up = 0x20,
    SonyRemoteButtonC1Down = 0x21,
} SonyRemoteButton;

typedef enum {
    SonyCameraStatusUnknown,
    SonyCameraStatusFocus,
    SonyCameraStatusShutter,
    SonyCameraStatusRecording,
} SonyCameraStatusType;

typedef struct {
    SonyCameraStatusType type;
    bool active;
} SonyCameraStatus;

/** Build the two-byte packet written to characteristic 0xFF01. */
void sony_protocol_button_packet(SonyRemoteButton button, uint8_t packet[2]);

/** Parse a three-byte notification received from characteristic 0xFF02. */
bool sony_protocol_parse_status(
    const uint8_t* data,
    size_t data_size,
    SonyCameraStatus* status);

/** True when a BLE AD structure contains Sony camera manufacturer data. */
bool sony_protocol_is_camera_advertisement(
    const uint8_t* advertisement,
    size_t advertisement_size,
    bool* pairing_enabled);

#include "sony_protocol.h"

#include <string.h>

#define BLE_AD_TYPE_MANUFACTURER_DATA 0xFF

/* UUID byte order used by the STM32WB ACI API. */
const uint8_t sony_remote_service_uuid[16] = {
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0x80,
};

void sony_protocol_button_packet(SonyRemoteButton button, uint8_t packet[2]) {
    packet[0] = 0x01;
    packet[1] = (uint8_t)button;
}

bool sony_protocol_parse_status(
    const uint8_t* data,
    size_t data_size,
    SonyCameraStatus* status) {
    if(!data || !status || data_size != 3 || data[0] != 0x02) return false;

    switch(data[1]) {
    case 0x3F:
        status->type = SonyCameraStatusFocus;
        break;
    case 0xA0:
        status->type = SonyCameraStatusShutter;
        break;
    case 0xD5:
        status->type = SonyCameraStatusRecording;
        break;
    default:
        return false;
    }

    status->active = data[2] == 0x20;
    return data[2] == 0x00 || data[2] == 0x20;
}

static bool sony_protocol_parse_manufacturer_data(
    const uint8_t* data,
    size_t data_size,
    bool* pairing_enabled) {
    static const uint8_t sony_camera_prefix[] = {0x2D, 0x01, 0x03, 0x00};
    if(data_size < sizeof(sony_camera_prefix) ||
       memcmp(data, sony_camera_prefix, sizeof(sony_camera_prefix)) != 0) {
        return false;
    }

    *pairing_enabled = false;
    for(size_t i = sizeof(sony_camera_prefix); i + 1 < data_size; ++i) {
        if(data[i] == 0x22) {
            const uint8_t flags = data[i + 1];
            *pairing_enabled = (flags & 0x42) == 0x42;
            break;
        }
    }
    return true;
}

bool sony_protocol_is_camera_advertisement(
    const uint8_t* advertisement,
    size_t advertisement_size,
    bool* pairing_enabled) {
    if(!advertisement || !pairing_enabled) return false;

    for(size_t offset = 0; offset < advertisement_size;) {
        const uint8_t field_size = advertisement[offset];
        if(field_size == 0) break;
        if(offset + (size_t)field_size >= advertisement_size) return false;

        const uint8_t field_type = advertisement[offset + 1];
        if(field_type == BLE_AD_TYPE_MANUFACTURER_DATA) {
            return sony_protocol_parse_manufacturer_data(
                &advertisement[offset + 2], field_size - 1, pairing_enabled);
        }
        offset += (size_t)field_size + 1;
    }
    return false;
}

#include "sony_transport.h"

#ifdef SONY_BLE_CENTRAL_INTERNAL

#include <ble/ble.h>
#include <furi.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_hal_bt.h>
#include <interface/patterns/ble_thread/tl/tl.h>

#include <string.h>

#define TAG "SonyTransport"

#define SONY_SCAN_INTERVAL        0x00A0U
#define SONY_SCAN_WINDOW          0x0050U
#define SONY_CONNECTION_INTERVAL  0x0018U
#define SONY_SUPERVISION_TIMEOUT  0x01F4U
#define SONY_INVALID_HANDLE       0xFFFFU
#define SONY_WRITE_TIMEOUT_MS     750U

#define SONY_GAP_GENERAL_DISCOVERY_PROC 0x02U
#define SONY_GATT_CCCD_UUID             0x2902U

typedef enum {
    SonyGattOperationNone,
    SonyGattOperationDiscoverService,
    SonyGattOperationDiscoverCommand,
    SonyGattOperationDiscoverStatus,
    SonyGattOperationDiscoverCccd,
    SonyGattOperationEnableNotifications,
    SonyGattOperationWriteButton,
} SonyGattOperation;

enum {
    SonyEventWriteComplete = (1U << 0),
    SonyEventDisconnected = (1U << 1),
};

struct SonyTransport {
    SonyTransportStateCallback state_callback;
    SonyTransportStatusCallback status_callback;
    void* context;

    GapSvcEventHandler* event_handler;
    FuriMutex* mutex;
    FuriEventFlag* events;

    SonyGattOperation operation;
    uint16_t connection_handle;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t command_handle;
    uint16_t status_handle;
    uint16_t status_cccd_handle;
    uint8_t peer_address_type;
    uint8_t peer_address[6];
    bool started;
    bool stopping;
    bool scanning;
    bool connection_pending;
    bool pairing_enabled;
    bool write_success;
};

static bool sony_transport_start_scan(SonyTransport* transport);

static void sony_transport_set_state(SonyTransport* transport, SonyTransportState state) {
    transport->state_callback(state, transport->context);
}

static void sony_transport_reset_connection(SonyTransport* transport) {
    transport->connection_handle = SONY_INVALID_HANDLE;
    transport->service_start_handle = 0;
    transport->service_end_handle = 0;
    transport->command_handle = 0;
    transport->status_handle = 0;
    transport->status_cccd_handle = 0;
    transport->operation = SonyGattOperationNone;
    transport->connection_pending = false;
    transport->pairing_enabled = false;
    transport->write_success = false;
}

static bool sony_transport_create_connection(SonyTransport* transport) {
    const tBleStatus status = aci_gap_create_connection(
        SONY_SCAN_INTERVAL,
        SONY_SCAN_WINDOW,
        transport->peer_address_type,
        transport->peer_address,
        GAP_PUBLIC_ADDR,
        SONY_CONNECTION_INTERVAL,
        SONY_CONNECTION_INTERVAL,
        0,
        SONY_SUPERVISION_TIMEOUT,
        0,
        0);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "Connection failed: %u", status);
        transport->connection_pending = false;
        sony_transport_set_state(transport, SonyTransportError);
        sony_transport_start_scan(transport);
        return false;
    }
    return true;
}

static bool sony_transport_start_scan(SonyTransport* transport) {
    if(!transport->started || transport->stopping) return false;

    sony_transport_reset_connection(transport);
    const tBleStatus status = aci_gap_start_general_discovery_proc(
        SONY_SCAN_INTERVAL, SONY_SCAN_WINDOW, GAP_PUBLIC_ADDR, 1);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "Scan failed: %u", status);
        sony_transport_set_state(transport, SonyTransportError);
        return false;
    }

    transport->scanning = true;
    sony_transport_set_state(transport, SonyTransportScanning);
    return true;
}

static bool sony_transport_begin_service_discovery(SonyTransport* transport) {
    UUID_t uuid = {0};
    memcpy(uuid.UUID_128, sony_remote_service_uuid, sizeof(uuid.UUID_128));
    transport->operation = SonyGattOperationDiscoverService;
    const tBleStatus status = aci_gatt_disc_primary_service_by_uuid(
        transport->connection_handle, UUID_TYPE_128, &uuid);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "Service discovery failed: %u", status);
        transport->operation = SonyGattOperationNone;
        sony_transport_set_state(transport, SonyTransportError);
        return false;
    }
    sony_transport_set_state(transport, SonyTransportDiscovering);
    return true;
}

static bool sony_transport_discover_characteristic(
    SonyTransport* transport,
    uint16_t uuid_value,
    SonyGattOperation operation) {
    UUID_t uuid = {.UUID_16 = uuid_value};
    transport->operation = operation;
    const tBleStatus status = aci_gatt_disc_char_by_uuid(
        transport->connection_handle,
        transport->service_start_handle,
        transport->service_end_handle,
        UUID_TYPE_16,
        &uuid);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "Characteristic %04X discovery failed: %u", uuid_value, status);
        transport->operation = SonyGattOperationNone;
        sony_transport_set_state(transport, SonyTransportError);
        return false;
    }
    return true;
}

static bool sony_transport_discover_cccd(SonyTransport* transport) {
    transport->operation = SonyGattOperationDiscoverCccd;
    const tBleStatus status = aci_gatt_disc_all_char_desc(
        transport->connection_handle,
        transport->status_handle,
        transport->service_end_handle);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "CCCD discovery failed: %u", status);
        transport->operation = SonyGattOperationNone;
        sony_transport_set_state(transport, SonyTransportError);
        return false;
    }
    return true;
}

static bool sony_transport_enable_notifications(SonyTransport* transport) {
    static const uint8_t notify_enabled[] = {0x01, 0x00};
    transport->operation = SonyGattOperationEnableNotifications;
    const tBleStatus status = aci_gatt_write_char_desc(
        transport->connection_handle,
        transport->status_cccd_handle,
        sizeof(notify_enabled),
        notify_enabled);
    if(status != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "Enabling notifications failed: %u", status);
        transport->operation = SonyGattOperationNone;
        sony_transport_set_state(transport, SonyTransportError);
        return false;
    }
    return true;
}

static void sony_transport_handle_advertising_report(
    SonyTransport* transport,
    const uint8_t* data,
    size_t data_size) {
    if(data_size < 1) return;

    const uint8_t report_count = data[0];
    size_t offset = 1;

    for(uint8_t report_index = 0; report_index < report_count; ++report_index) {
        /*
         * HCI LE advertising reports are variable length. Validate each
         * record before examining it because malformed reports must never
         * allow an out-of-bounds read in the BLE event thread.
         */
        if(offset + 10 > data_size) return;
        const uint8_t address_type = data[offset + 1];
        const uint8_t* address = &data[offset + 2];
        const uint8_t advertisement_size = data[offset + 8];
        if(offset + (size_t)10 + advertisement_size > data_size) return;
        const uint8_t* advertisement = &data[offset + 9];
        bool pairing_enabled = false;

        if(sony_protocol_is_camera_advertisement(
               advertisement, advertisement_size, &pairing_enabled)) {
            transport->pairing_enabled = pairing_enabled;
            transport->peer_address_type = address_type;
            memcpy(transport->peer_address, address, sizeof(transport->peer_address));
            transport->connection_pending = true;
            transport->scanning = false;
            sony_transport_set_state(transport, SonyTransportConnecting);

            const tBleStatus status =
                aci_gap_terminate_gap_proc(SONY_GAP_GENERAL_DISCOVERY_PROC);
            if(status != BLE_STATUS_SUCCESS) {
                FURI_LOG_E(TAG, "Stopping discovery failed: %u", status);
                transport->connection_pending = false;
                sony_transport_set_state(transport, SonyTransportError);
                sony_transport_start_scan(transport);
            }
            return;
        }

        offset += (size_t)10 + advertisement_size;
    }
}

static void sony_transport_handle_gap_complete(
    SonyTransport* transport,
    const aci_gap_proc_complete_event_rp0* event) {
    if(event->Procedure_Code != SONY_GAP_GENERAL_DISCOVERY_PROC ||
       !transport->connection_pending) {
        return;
    }

    if(event->Status == BLE_STATUS_SUCCESS) {
        sony_transport_create_connection(transport);
    } else {
        FURI_LOG_E(TAG, "Discovery termination failed: %u", event->Status);
        transport->connection_pending = false;
        sony_transport_set_state(transport, SonyTransportError);
        sony_transport_start_scan(transport);
    }
}

static void sony_transport_handle_connected(
    SonyTransport* transport,
    const hci_le_connection_complete_event_rp0* event) {
    if(event->Status != BLE_STATUS_SUCCESS || event->Role != 0) {
        FURI_LOG_E(TAG, "Central connection failed: status=%u role=%u", event->Status, event->Role);
        sony_transport_set_state(transport, SonyTransportError);
        sony_transport_start_scan(transport);
        return;
    }

    transport->connection_handle = event->Connection_Handle;
    if(transport->pairing_enabled) {
        sony_transport_set_state(transport, SonyTransportPairing);
        const tBleStatus status = aci_gap_send_pairing_req(event->Connection_Handle, 0);
        if(status != BLE_STATUS_SUCCESS) {
            FURI_LOG_E(TAG, "Pairing request failed: %u", status);
            sony_transport_set_state(transport, SonyTransportError);
        }
    } else {
        sony_transport_begin_service_discovery(transport);
    }
}

static void sony_transport_handle_disconnected(
    SonyTransport* transport,
    const hci_disconnection_complete_event_rp0* event) {
    if(event->Connection_Handle != transport->connection_handle) return;

    sony_transport_reset_connection(transport);
    transport->scanning = false;
    furi_event_flag_set(transport->events, SonyEventDisconnected | SonyEventWriteComplete);
    if(transport->stopping) return;

    sony_transport_set_state(transport, SonyTransportDisconnected);
    sony_transport_start_scan(transport);
}

static void sony_transport_handle_service_result(
    SonyTransport* transport,
    const aci_att_find_by_type_value_resp_event_rp0* event) {
    if(event->Connection_Handle != transport->connection_handle ||
       transport->operation != SonyGattOperationDiscoverService ||
       event->Num_of_Handle_Pair == 0) {
        return;
    }

    transport->service_start_handle =
        event->Attribute_Group_Handle_Pair[0].Found_Attribute_Handle;
    transport->service_end_handle = event->Attribute_Group_Handle_Pair[0].Group_End_Handle;
}

static void sony_transport_handle_characteristic_result(
    SonyTransport* transport,
    const aci_gatt_disc_read_char_by_uuid_resp_event_rp0* event) {
    if(event->Connection_Handle != transport->connection_handle ||
       event->Attribute_Value_Length < 3) {
        return;
    }

    const uint16_t value_handle =
        (uint16_t)event->Attribute_Value[1] | ((uint16_t)event->Attribute_Value[2] << 8);
    if(transport->operation == SonyGattOperationDiscoverCommand) {
        transport->command_handle = value_handle;
    } else if(transport->operation == SonyGattOperationDiscoverStatus) {
        transport->status_handle = value_handle;
    }
}

static void sony_transport_handle_descriptor_result(
    SonyTransport* transport,
    const aci_att_find_info_resp_event_rp0* event) {
    if(event->Connection_Handle != transport->connection_handle ||
       transport->operation != SonyGattOperationDiscoverCccd || event->Format != 1) {
        return;
    }

    for(size_t offset = 0; offset + 3 < event->Event_Data_Length; offset += 4) {
        const uint16_t handle = (uint16_t)event->Handle_UUID_Pair[offset] |
                                ((uint16_t)event->Handle_UUID_Pair[offset + 1] << 8);
        const uint16_t uuid = (uint16_t)event->Handle_UUID_Pair[offset + 2] |
                              ((uint16_t)event->Handle_UUID_Pair[offset + 3] << 8);
        if(uuid == SONY_GATT_CCCD_UUID) {
            transport->status_cccd_handle = handle;
            return;
        }
    }
}

static void sony_transport_handle_gatt_complete(
    SonyTransport* transport,
    const aci_gatt_proc_complete_event_rp0* event) {
    if(event->Connection_Handle != transport->connection_handle) return;

    const SonyGattOperation completed_operation = transport->operation;
    transport->operation = SonyGattOperationNone;

    if(completed_operation == SonyGattOperationWriteButton) {
        transport->write_success = event->Error_Code == BLE_STATUS_SUCCESS;
        furi_event_flag_set(transport->events, SonyEventWriteComplete);
        if(event->Error_Code != BLE_STATUS_SUCCESS) {
            FURI_LOG_E(TAG, "Button write failed: %u", event->Error_Code);
        }
        return;
    }

    if(event->Error_Code != BLE_STATUS_SUCCESS) {
        FURI_LOG_E(TAG, "GATT operation %u failed: %u", completed_operation, event->Error_Code);
        sony_transport_set_state(transport, SonyTransportError);
        return;
    }

    switch(completed_operation) {
    case SonyGattOperationDiscoverService:
        if(transport->service_start_handle && transport->service_end_handle) {
            sony_transport_discover_characteristic(
                transport, 0xFF01, SonyGattOperationDiscoverCommand);
        } else {
            sony_transport_set_state(transport, SonyTransportError);
        }
        break;
    case SonyGattOperationDiscoverCommand:
        if(transport->command_handle) {
            sony_transport_discover_characteristic(
                transport, 0xFF02, SonyGattOperationDiscoverStatus);
        } else {
            sony_transport_set_state(transport, SonyTransportError);
        }
        break;
    case SonyGattOperationDiscoverStatus:
        if(transport->status_handle) {
            sony_transport_discover_cccd(transport);
        } else {
            sony_transport_set_state(transport, SonyTransportError);
        }
        break;
    case SonyGattOperationDiscoverCccd:
        if(transport->status_cccd_handle) {
            sony_transport_enable_notifications(transport);
        } else {
            sony_transport_set_state(transport, SonyTransportError);
        }
        break;
    case SonyGattOperationEnableNotifications:
        sony_transport_set_state(transport, SonyTransportReady);
        break;
    default:
        break;
    }
}

static BleEventAckStatus sony_transport_event_callback(void* payload, void* context) {
    SonyTransport* transport = context;
    hci_event_pckt* packet = (hci_event_pckt*)((hci_uart_pckt*)payload)->data;

    if(!transport->started) return BleEventNotAck;

    if(packet->evt == HCI_DISCONNECTION_COMPLETE_EVT_CODE) {
        hci_disconnection_complete_event_rp0* event = (void*)packet->data;
        if(event->Connection_Handle == transport->connection_handle) {
            sony_transport_handle_disconnected(transport, event);
            return BleEventAckFlowEnable;
        }
    } else if(packet->evt == HCI_LE_META_EVT_CODE) {
        evt_le_meta_event* meta = (void*)packet->data;
        if(meta->subevent == HCI_LE_ADVERTISING_REPORT_SUBEVT_CODE && transport->scanning) {
            if(packet->plen < 1) return BleEventNotAck;
            sony_transport_handle_advertising_report(transport, meta->data, packet->plen - 1);
            return BleEventAckFlowEnable;
        } else if(meta->subevent == HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE) {
            sony_transport_handle_connected(transport, (void*)meta->data);
            return BleEventAckFlowEnable;
        }
    } else if(packet->evt == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
        evt_blecore_aci* event = (void*)packet->data;
        switch(event->ecode) {
        case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE: {
            aci_gap_numeric_comparison_value_event_rp0* comparison = (void*)event->data;
            if(comparison->Connection_Handle == transport->connection_handle) {
                aci_gap_numeric_comparison_value_confirm_yesno(
                    transport->connection_handle, 1);
                return BleEventAckFlowEnable;
            }
            break;
        }
        case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE: {
            aci_gap_pairing_complete_event_rp0* pairing = (void*)event->data;
            if(pairing->Connection_Handle == transport->connection_handle) {
                if(pairing->Status == BLE_STATUS_SUCCESS) {
                    sony_transport_begin_service_discovery(transport);
                } else {
                    FURI_LOG_E(TAG, "Pairing failed: %u reason=%u", pairing->Status, pairing->Reason);
                    sony_transport_set_state(transport, SonyTransportError);
                }
                return BleEventAckFlowEnable;
            }
            break;
        }
        case ACI_GAP_PROC_COMPLETE_VSEVT_CODE:
            sony_transport_handle_gap_complete(transport, (void*)event->data);
            return BleEventAckFlowEnable;
        case ACI_ATT_FIND_BY_TYPE_VALUE_RESP_VSEVT_CODE:
            sony_transport_handle_service_result(transport, (void*)event->data);
            return BleEventAckFlowEnable;
        case ACI_GATT_DISC_READ_CHAR_BY_UUID_RESP_VSEVT_CODE:
            sony_transport_handle_characteristic_result(transport, (void*)event->data);
            return BleEventAckFlowEnable;
        case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
            sony_transport_handle_descriptor_result(transport, (void*)event->data);
            return BleEventAckFlowEnable;
        case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
            sony_transport_handle_gatt_complete(transport, (void*)event->data);
            return BleEventAckFlowEnable;
        case ACI_GATT_NOTIFICATION_VSEVT_CODE: {
            aci_gatt_notification_event_rp0* notification = (void*)event->data;
            if(notification->Connection_Handle == transport->connection_handle &&
               notification->Attribute_Handle == transport->status_handle) {
                SonyCameraStatus status;
                if(sony_protocol_parse_status(
                       notification->Attribute_Value,
                       notification->Attribute_Value_Length,
                       &status)) {
                    transport->status_callback(status, transport->context);
                }
                return BleEventAckFlowEnable;
            }
            break;
        }
        default:
            break;
        }
    }

    return BleEventNotAck;
}

SonyTransport* sony_transport_alloc(
    SonyTransportStateCallback state_callback,
    SonyTransportStatusCallback status_callback,
    void* context) {
    furi_check(state_callback);
    furi_check(status_callback);

    SonyTransport* transport = malloc(sizeof(SonyTransport));
    memset(transport, 0, sizeof(SonyTransport));
    transport->state_callback = state_callback;
    transport->status_callback = status_callback;
    transport->context = context;
    transport->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    transport->events = furi_event_flag_alloc();
    sony_transport_reset_connection(transport);
    return transport;
}

void sony_transport_free(SonyTransport* transport) {
    furi_check(transport);
    furi_check(!transport->started);
    furi_event_flag_free(transport->events);
    furi_mutex_free(transport->mutex);
    free(transport);
}

bool sony_transport_start(SonyTransport* transport) {
    furi_check(transport);
    if(transport->started) return true;

    transport->started = true;
    transport->stopping = false;
    transport->event_handler =
        ble_event_dispatcher_register_svc_handler(sony_transport_event_callback, transport);

    furi_hal_bt_stop_advertising();
    furi_delay_ms(100);
    return sony_transport_start_scan(transport);
}

void sony_transport_stop(SonyTransport* transport) {
    furi_check(transport);
    if(!transport->started) return;

    transport->stopping = true;
    if(transport->scanning) {
        aci_gap_terminate_gap_proc(SONY_GAP_GENERAL_DISCOVERY_PROC);
        transport->scanning = false;
    }

    if(transport->connection_handle != SONY_INVALID_HANDLE) {
        furi_event_flag_clear(transport->events, SonyEventDisconnected);
        aci_gap_terminate(transport->connection_handle, 0x13);
        furi_event_flag_wait(
            transport->events, SonyEventDisconnected, FuriFlagWaitAny, SONY_WRITE_TIMEOUT_MS);
    }

    transport->started = false;
    ble_event_dispatcher_unregister_svc_handler(transport->event_handler);
    transport->event_handler = NULL;
    sony_transport_reset_connection(transport);
    furi_hal_bt_start_advertising();
}

bool sony_transport_send_button(SonyTransport* transport, SonyRemoteButton button) {
    furi_check(transport);
    if(furi_mutex_acquire(transport->mutex, SONY_WRITE_TIMEOUT_MS) != FuriStatusOk) return false;

    bool success = false;
    if(transport->started && !transport->stopping &&
       transport->connection_handle != SONY_INVALID_HANDLE && transport->command_handle &&
       transport->operation == SonyGattOperationNone) {
        uint8_t packet[2];
        sony_protocol_button_packet(button, packet);
        furi_event_flag_clear(transport->events, SonyEventWriteComplete);
        transport->write_success = false;
        transport->operation = SonyGattOperationWriteButton;
        const tBleStatus status = aci_gatt_write_char_value(
            transport->connection_handle, transport->command_handle, sizeof(packet), packet);
        if(status == BLE_STATUS_SUCCESS) {
            const uint32_t flags = furi_event_flag_wait(
                transport->events,
                SonyEventWriteComplete,
                FuriFlagWaitAny,
                SONY_WRITE_TIMEOUT_MS);
            success = (flags & SonyEventWriteComplete) != 0 && transport->write_success &&
                      transport->connection_handle != SONY_INVALID_HANDLE;
        } else {
            FURI_LOG_E(TAG, "Button command rejected: %u", status);
            transport->operation = SonyGattOperationNone;
        }
    }

    furi_mutex_release(transport->mutex);
    return success;
}

#endif /* SONY_BLE_CENTRAL_INTERNAL */

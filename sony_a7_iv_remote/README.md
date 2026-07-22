# Sony A7 IV Remote for Flipper Zero

This project is the in-progress Sony camera remote. It targets the Bluetooth
remote mode used by Sony's RMT-P1BT and the ILCE-7M4 (A7 IV).

## Implemented

- Sony camera advertisement recognition, including pairing-mode flags.
- Sony remote service UUID (`8000FF00-FF00-FFFF-FFFF-FFFFFFFFFFFF`).
- Remote command characteristic (`0xFF01`) packet encoding.
- Camera status characteristic (`0xFF02`) notification parsing.
- Correct four-command shutter sequence.
- Record button down/up sequence.
- Flipper UI and connection-state model.

## Current transport limitation

Sony cameras are BLE peripherals. The remote must scan and connect as a BLE
central/GATT client. Current Flipper and Momentum external-app APIs expose BLE
peripheral profiles and advertising, but do not expose central scanning,
connection, service discovery, or characteristic writes.

For that reason, this revision deliberately reports `Central firmware required`
instead of advertising a fake keyboard or claiming to control the camera. The
next implementation unit is a small Momentum firmware extension that enables
the combined peripheral/central GAP role and exposes a Sony GATT-client adapter
to this app.

## Intended controls

| Flipper button | Camera operation |
| --- | --- |
| Up | Focus, release shutter, and restore the shutter state |
| OK | Toggle movie recording |
| Back (hold) | Exit |

## Camera setup for pairing

On the A7 IV, enable Bluetooth and Bluetooth remote control, then open the
camera's Bluetooth pairing screen. Pairing must be confirmed on the camera.

The exact menu wording can vary by camera firmware, but it is under
`Network` / `Transfer/Remote` / `Bluetooth Rmt Ctrl` and `Bluetooth Settings`.

## Protocol notes

Commands written to `0xFF01` are two-byte packets: `01 <command>`. A shutter
activation must use `01 07`, `01 09`, `01 06`, `01 08` in that order. Record
uses `01 0F`, then `01 0E`. Notifications from `0xFF02` use `02 <property>
<state>`, where properties `3F`, `A0`, and `D5` represent focus, shutter, and
recording.

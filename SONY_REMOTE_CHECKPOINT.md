# Sony A7 IV Remote — Work Checkpoint

Checkpoint created: 2026-07-22 before restarting the computer.

## Completed

- Inspected the existing Flipper BLE HID remotes and the unfinished Sony scaffold.
- Verified that the Sony A7 IV uses the RMT-P1BT-style BLE protocol.
- Verified the Sony remote service UUID:
  `8000FF00-FF00-FFFF-FFFF-FFFFFFFFFFFF`.
- Verified command characteristic `0xFF01` and notification characteristic
  `0xFF02`.
- Verified shutter sequence: `01 07`, `01 09`, `01 06`, `01 08`.
- Verified record sequence: `01 0F`, `01 0E`.
- Created the new `sony_a7_iv_remote` app project.
- Implemented advertisement recognition, protocol packet generation, camera
  status parsing, the connection-state UI, and transport abstraction.
- Updated uFBT to the Momentum `mntm-012` SDK.

## Important finding

The stock/Momentum external-app API exposes BLE peripheral/advertising
features, but the Sony camera requires the Flipper to be a BLE central/GATT
client. The current placeholder transport intentionally displays
`Central firmware required`. The next implementation unit is a firmware-side
central adapter; the protocol and UI layers are ready to use it.

## Interrupted operation

The official uFBT Windows toolchain downloader was running because the local
`C:\Users\logan.williams\.ufbt\toolchain\x86_64-windows` directory was empty.
It was deliberately terminated before reboot. It is safe to rerun.

## Resume here

1. Check whether the toolchain finished enough to contain `VERSION` and
   `bin\arm-none-eabi-gcc.exe`.
2. If missing, rerun uFBT's official toolchain downloader for version 39.
3. Run `ufbt` inside `sony_a7_iv_remote` and fix any compiler issues.
4. Implement the firmware-side combined central/peripheral GAP support and
   Sony GATT client transport.
5. Build again, then perform camera pairing and hardware tests.

## Active implementation plan

1. Repository baseline — complete.
2. Sony A7 IV protocol verification — complete.
3. Protocol/transport separation and lifecycle — in progress.
4. End-to-end shutter and record controls — pending BLE central transport.
5. Momentum build, install documentation, and hardware validation — pending.

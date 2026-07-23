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
- Cloned Momentum `dev` at the planned `8ed809fba8af7ac3f09b9495a597d8963f9178a8`
  baseline.
- Enabled Momentum's combined BLE peripheral/central GAP role.
- Integrated the Sony remote as a built-in Momentum app and enabled its
  central/GATT transport.
- Installed the official Momentum ARM toolchain and completed successful
  `./fbt firmware_all` and `./fbt fw_dist` builds.

## Important finding

The stock/Momentum external-app API exposes BLE peripheral/advertising features,
but the Sony camera requires the Flipper to be a BLE central/GATT client. The
app is consequently a built-in component of a custom Momentum build, not an
external FAP.

## Resume here

1. Install the generated full-firmware `.dfu` from `Momentum-Firmware/dist/f7-C/`.
2. On the A7 IV, enable Bluetooth Remote Control and open the Bluetooth pairing
   screen.
3. Open **Sony A7 IV Remote** on the Flipper and approve pairing on the camera
   if prompted.
4. Verify the Up shutter sequence and the OK recording toggle, then capture
   any connection or GATT errors from the Flipper log.

## Active implementation plan

1. Repository baseline — complete.
2. Sony A7 IV protocol verification — complete.
3. Protocol/transport separation and lifecycle — complete.
4. End-to-end shutter and record controls — implementation complete; hardware
   validation pending.
5. Momentum build and install documentation — complete; hardware validation
   pending.

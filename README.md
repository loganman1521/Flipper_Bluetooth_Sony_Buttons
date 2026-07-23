# Flipper Bluetooth Sony Buttons

Experimental Bluetooth projects for the Flipper Zero, including presentation
and shortcut remotes plus an early Sony camera remote scaffold.

## Included apps

- `Flipper Bluetooth controls/Flipper Bluetooth controls/bluetooth-remote-with-enter-key/enter_remote`
  - BLE presentation remote; OK sends Enter and the directional buttons send arrow keys.
- `Flipper bluetooth Button control/Flipper bluetooth Button control/enter_remote`
  - Earlier Enter Remote variant.
- `Flipper Bluetooth controls/Flipper Bluetooth controls/bluetooth-remote-rv-mc-inv/rvmc_remote`
  - BLE keyboard remote; Up types `RV`, Down types `MC`, and OK types `INV`.
- `sony_camera_remote`
  - Early UI and input scaffold for a Sony camera remote. Bluetooth command handling
    and Sony camera protocol support are not implemented yet.
- `sony_a7_iv_remote`
  - Sony A7 IV remote with a firmware-coupled BLE central/GATT client. It is
    built into the custom Momentum firmware, not deployed as a standalone FAP.

## Building

The Enter Remote and RV/MC/INV apps are external apps built with
[ufbt](https://github.com/flipperdevices/flipperzero-ufbt) against a matching
Momentum firmware SDK. Change into an app directory containing `application.fam`,
then run:

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
ufbt
```

The compiled `.fap` is written to that app's `dist/` directory. Build outputs are
intentionally ignored by Git; rebuild them for the firmware version on your Flipper.

The Sony A7 IV app instead requires the custom Momentum firmware build described
in [`MOMENTUM_SETUP.md`](MOMENTUM_SETUP.md).

Each app's README contains its controls, installation instructions, and troubleshooting notes.

## Status

The Enter Remote and RV/MC/INV Remote are functional external-app projects. The
older Sony Camera Remote is exploratory. The active Sony work is in
`sony_a7_iv_remote`; its firmware integration now compiles, while camera pairing
and hardware validation remain to be performed.

## Momentum firmware source

The local Momentum checkout used for Sony BLE-central support is intentionally
not committed to this repository. Follow [`MOMENTUM_SETUP.md`](MOMENTUM_SETUP.md)
to recreate the matching checkout.

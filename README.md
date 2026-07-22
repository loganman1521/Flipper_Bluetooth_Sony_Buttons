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
  - Sony A7 IV remote project with the Sony command protocol, controls UI, and
    a firmware-integration boundary. It needs the planned Momentum BLE-central
    firmware extension before it can connect to a camera.

## Building

The Flipper apps are external apps built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)
against a matching Momentum firmware SDK. Change into an app directory containing
`application.fam`, then run:

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
ufbt
```

The compiled `.fap` is written to that app's `dist/` directory. Build outputs are
intentionally ignored by Git; rebuild them for the firmware version on your Flipper.

Each app's README contains its controls, installation instructions, and troubleshooting notes.

## Status

The Enter Remote and RV/MC/INV Remote are functional external-app projects. The
older Sony Camera Remote is exploratory. The active Sony work is in
`sony_a7_iv_remote`; see its README and `SONY_REMOTE_CHECKPOINT.md` for the
current implementation status.

## Momentum firmware source

The Momentum checkout used to build the forthcoming Sony BLE-central support is
intentionally not included in this repository. Follow
[`MOMENTUM_SETUP.md`](MOMENTUM_SETUP.md) to recreate the matching checkout.

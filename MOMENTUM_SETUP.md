# Local Momentum Firmware Setup

The Sony A7 IV remote needs a custom Momentum firmware build because the camera
is a BLE peripheral and the Flipper must act as a BLE central/GATT client.

The source checkout is deliberately excluded from this repository. Clone the
same upstream branch beside this project:

```powershell
git clone --recursive --branch dev https://github.com/Next-Flip/Momentum-Firmware.git Momentum-Firmware
```

The planning checkout used commit `8ed809fba8af7ac3f09b9495a597d8963f9178a8`.
To use exactly that baseline after cloning:

```powershell
cd Momentum-Firmware
git checkout 8ed809fba8af7ac3f09b9495a597d8963f9178a8
git submodule update --init --recursive
```

## Sony A7 IV firmware integration

The Sony app is intentionally built into Momentum, rather than as a `.fap`:
its BLE central/GATT-client calls are firmware-private APIs. In this workspace,
`Momentum-Firmware/applications_user/sony_a7_iv_remote` is a symlink to the
parent project's `sony_a7_iv_remote` source folder.

Create that local link after cloning Momentum:

```bash
cd Momentum-Firmware
ln -s ../../sony_a7_iv_remote applications_user/sony_a7_iv_remote
```

The Momentum changes are:

- enable the combined `GAP_PERIPHERAL_ROLE | GAP_CENTRAL_ROLE` at startup;
- include `sony_a7_iv_remote` in the main app package; and
- compile the Sony app with `SONY_BLE_CENTRAL_INTERNAL`.

Build the installable firmware from the Momentum checkout:

```bash
cd Momentum-Firmware
./fbt fw_dist
```

The full firmware `.dfu` and `.bin` files are written to `dist/f7-C/`. Install
the `.dfu` using Momentum's normal firmware-update process. The first build
downloads the official ARM toolchain if it is not already present.

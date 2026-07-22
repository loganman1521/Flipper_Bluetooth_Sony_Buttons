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

The firmware patch has not been made yet. Keep the checkout clean until the
Sony central-client implementation begins; the project checkpoint and plan are
in the parent repository.

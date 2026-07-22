# RV/MC/INV Remote (BLE) — Flipper Zero app

A Bluetooth LE keyboard remote for the Flipper Zero that types short
**capitalized text shortcuts** instead of acting like a presentation clicker.
It is a separate app from the *Enter Remote (BLE)* app and can be installed and
paired alongside it.

It builds as a normal external app (`.fap`) for **Momentum** firmware — no firmware
rebuild needed. The Flipper's BLE keyboard functions (`ble_profile_hid_*`) are private
in the firmware's API table, but `application.fam` statically links them into the app
via `fap_libs=["ble_profile"]` — the same way the stock Bluetooth Remote app does it.

## Controls

| Flipper button  | Types   |
| --------------- | ------- |
| **Up**          | `RV`    |
| **Down**        | `MC`    |
| **OK** (center) | `INV`   |
| **Back** (hold) | — exits the app |

Each letter is sent as **Shift + letter**, so the text always arrives
capitalized regardless of the host's Caps Lock state. Left, Right, and a short
Back tap do nothing.

When a computer is connected, the Flipper's LED glows blue (same as the stock remote).

## Its own Bluetooth identity

The app advertises under its **own name and MAC address** (name prefix `RVMC`, set via
`BleProfileHidParams` in `rvmc_remote.c`), separate from the Flipper's serial Bluetooth,
the stock Bluetooth Remote, and the Enter Remote app. Each app having a unique address
means the host sees each one as a brand-new device, so they pair cleanly and coexist —
reusing an identity would make a host with an existing bond auto-reconnect with
mismatched pairing keys, which shows up as a rapid connect/disconnect loop.

---

## Build & install (Ubuntu / Linux)

The build tool is **ufbt** (micro Flipper Build Tool), pointed at the **Momentum SDK**
so the app matches the firmware on the device.

### 1. Install ufbt (one time)

Ubuntu's Python is externally managed, so install into a venv:

```bash
python3 -m venv ~/.venvs/ufbt
~/.venvs/ufbt/bin/pip install ufbt
mkdir -p ~/.local/bin && ln -sf ~/.venvs/ufbt/bin/ufbt ~/.local/bin/ufbt
```

### 2. Point ufbt at the Momentum SDK (one time, and after firmware updates)

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
```

Use `--channel=release` for stable Momentum (e.g. `mntm-012`), or
`--channel=development` if your Flipper runs a Momentum dev build. The SDK API
version must match the installed firmware, so rerun this after updating the
firmware on the Flipper.

### 3. Build the app

From inside the `rvmc_remote` folder (the one containing `application.fam`):

```bash
ufbt
```

The result is `dist/rvmc_remote.fap`.

### 4. Put it on the Flipper

**Option A — plug in and launch (easiest):** connect the Flipper by USB, close
qFlipper/anything else holding the serial port, then:

```bash
ufbt launch
```

This uploads the `.fap` to `SD Card/apps/Bluetooth/` and starts it immediately.

**Option B — copy the file manually:** copy `dist/rvmc_remote.fap` onto the SD card
under `apps/Bluetooth/` using [qFlipper](https://flipperzero.one/update) (drag‑and‑drop
in the file browser). On the Flipper it appears under **Apps → Bluetooth → RV/MC/INV Remote (BLE)**.

---

## Using it

1. On the Flipper: **Apps → Bluetooth → RV/MC/INV Remote (BLE)**. It starts advertising.
2. On your computer/phone: open **Bluetooth settings**, find the device whose name starts
   with **`RVMC`** (e.g. `RVMC <name>`) and pair it. It connects as a keyboard.
3. Put the cursor in a text field and press **Up**, **Down**, or **OK**.
4. Hold **Back** to quit the app (this also restores the Flipper's normal Bluetooth).

---

## Customizing the text each button types

Open [`rvmc_remote.c`](rvmc_remote.c) and look at the key arrays at the top of
`rvmc_remote_handle_input()`:

```c
static const uint16_t rv_keys[] = {KEY_R, KEY_V};
static const uint16_t mc_keys[] = {KEY_M, KEY_C};
static const uint16_t inv_keys[] = {KEY_I, KEY_N, KEY_V};
```

Add or change letters using [USB HID usage IDs](https://usb.org/sites/default/files/hut1_5.pdf)
(`A` = 0x04 through `Z` = 0x1D; define new `KEY_x` constants near the top of the
file). Letters are typed with Shift held; to type lowercase, remove
`MOD_LEFT_SHIFT` in `rvmc_remote_type_keys()`. Rebuild with `ufbt` after editing.

---

## Troubleshooting

- **"missing symbol" / won't launch after copying:** the `.fap` must match the firmware
  on the Flipper. Rerun step 2 (with the channel your Flipper actually runs), then
  rebuild with `ufbt`.
- **Computer won't pair:** delete any existing "Flipper …" Bluetooth pairing on the
  computer, make sure no other app is using the Flipper's Bluetooth, and try again.
- **Pairing shows the passkey, you accept on both sides, then Windows says "Try connecting
  your device again":** this is a stuck/stale bond, not a code problem. Clear the bond on
  *both* ends: on Windows remove the device (Settings → Bluetooth → the `RVMC …` device →
  Remove device), and on the Flipper go to **Settings → Bluetooth → Forget All Paired
  Devices**. Then reopen the app and pair fresh. Restarting the PC alone does **not**
  clear the bond, so it won't fix this on its own.
- **App crashes on open:** bump `stack_size` in `application.fam` from `2 * 1024` to
  `4 * 1024` and rebuild.
- **Nothing gets typed:** confirm the LED is blue (connected) and that a text field
  has keyboard focus on the host.

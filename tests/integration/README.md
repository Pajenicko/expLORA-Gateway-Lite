# expLORA Gateway integration tests

End-to-end Python smoke test for a running expLORA Gateway Lite device.
Complements the host-side `pio test -e native` unit tests (which only
cover pure logic in isolation) by exercising the full RX path on real
hardware: HTTP API, web UI, WiFi provisioning, LoRa decode, calibration.

## Quickstart (config-file mode, recommended)

```bash
# 1. Install Python deps
pip install requests pyserial

# 2. Copy the template, fill in your values
cp tests/integration/config.example.ini tests/integration/config.ini
$EDITOR tests/integration/config.ini

# 3. Plug the gateway into your Mac via USB and run
python3 tests/integration/test_gateway.py --config tests/integration/config.ini
```

`config.ini` is gitignored — your home WiFi password and sensor device key
never get committed.

## What it tests

| Section | Cases | What it covers |
|---|---|---|
| Read-only smoke | 10 | `/firmware/version`, `/api?format=json|csv`, `/sensors` (incl. new health columns), `/config`, `/`, `/diag/heap`, `/logs`, 404 handling |
| POST validation | 4  | Regression guard for the input validation added in 1.1.6 (empty SSID, short password, missing param, SSID > 32 chars) |
| Live sensor round-trip | 4 | Adds the sensor → waits up to 20 min for a real LoRa packet → checks decoded values are in physical ranges → confirms sensor appears in `/sensors` HTML |
| Reboot recovery | 1 | `GET /reboot` → waits for device to drop off → waits up to 60 s for it to come back with the same version |

19 tests total when fully configured (sensor SN set, reboot enabled).

## Provisioning flow (first run, gateway with no STA config)

When `[gateway].ip` is empty or unreachable, the script provisions the
device automatically:

1. Reads UART to discover the actual AP SSID (`expLORA-GW-XXXXXX`)
2. macOS: switches WiFi via `networksetup -setairportnetwork en0 <ssid>`
   Linux/Windows: prints `Connect to <ssid>` and waits for ENTER
3. POSTs `[wifi].ssid` + `[wifi].password` to `http://192.168.4.1/config`
4. Watches UART for `expLORA Gateway Lite starting up` (= reboot)
5. Watches UART for `WiFi connected! IP: x.x.x.x` (= STA joined)
6. Switches host WiFi back to home
7. Runs the test suite against the discovered IP

After the first successful run, paste the discovered IP into
`[gateway].ip` of `config.ini` and subsequent runs skip provisioning
entirely (sub-second startup).

## Config file format

See `config.example.ini` for the full template with comments. Minimal
required fields:

```ini
[wifi]
ssid = my-home-wifi
password = my-home-wifi-password

[test_sensor]
sn = 475481
key = D43C2780
type = 1
```

If `[gateway].ip` is empty, the script will provision. If `[test_sensor].sn`
is empty, the live-packet test is skipped.

## CLI overrides

Any value can also be passed on the command line (CLI wins over config):

```bash
python3 tests/integration/test_gateway.py --config tests/integration/config.ini \
    --packet-timeout 600 \
    --skip-reboot
```

## Always start from current firmware

Use the `[flash]` config section (or CLI flags) to reflash the device at
the start of each run. Three independent toggles:

| Mode | Effect |
|---|---|
| `before_test = true` | `pio run -e <env> -t upload` — preserves LittleFS so saved WiFi config still works on reboot |
| `+ rebuild = true`   | `pio run -e <env>` first — picks up any source edits |
| `+ erase = true`     | `pio run -e <env> -t erase` first — wipes LittleFS too, **forces re-provisioning** every run |

CLI equivalents:

```bash
# Reflash with whatever firmware.bin is already built
python3 tests/integration/test_gateway.py --config tests/integration/config.ini --flash

# Rebuild + flash + run
python3 tests/integration/test_gateway.py --config tests/integration/config.ini --rebuild

# Truly fresh every run (re-provisions WiFi from scratch)
python3 tests/integration/test_gateway.py --config tests/integration/config.ini --rebuild --erase
```

`--rebuild` and `--erase` imply `--flash`. The script locates the `pio`
binary at `~/.platformio/penv/bin/pio` (or PATH), and project root by
walking up until it finds `platformio.ini`.

## Legacy mode (no config file)

Backwards-compatible CLI-only mode (assumes gateway already on the home
network, no UART, no WiFi switching):

```bash
python3 tests/integration/test_gateway.py \
    --url http://192.168.1.42 \
    --test-sensor-sn 475481 \
    --test-sensor-key D43C2780
```

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `no serial port — set [gateway].serial_port` | Device not plugged in / wrong USB cable (charge-only) / autodetect missed it | Plug in via data cable; or set `[gateway].serial_port = /dev/cu.usbmodem...` |
| `WiFi switched but http://192.168.4.1 unreachable` | Gateway didn't actually broadcast AP, or macOS associated with a different network | Power-cycle the gateway; check `[gateway].wifi_interface` (usually `en0`) |
| `device rebooted but did not join home WiFi within 30 s` | Wrong WiFi password, SSID typo, or gateway out of range of router | Double-check `[wifi].ssid` + `[wifi].password` |
| `no fresh packet from sensor … within 1200s` | Sensor offline / wrong key / out of range / too aggressive `packet_timeout` | Confirm sensor is alive (CR2032 battery), key matches gateway-side, increase `[options].packet_timeout` |
| `networksetup` prompts for password | macOS hasn't seen this network before | Type your user password once; subsequent runs are passwordless |

## What the script never does

- **Never commits credentials.** `config.ini` is gitignored.
- **Never deletes sensors.** Adding the test sensor is idempotent — re-runs
  skip the add step. Remove the sensor manually via `/sensors` if you want
  it gone.
- **Never flashes firmware.** The reboot test only restarts the running
  binary; OTA flow is not exercised.

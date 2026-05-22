#!/usr/bin/env python3
"""
Integration smoke test for a running expLORA Gateway Lite device.

Hits the device over HTTP and asserts response status, content type,
JSON shape, validation behaviour, and (optionally) reboot recovery.

Requirements:
    pip install requests

Usage:
    # Basic smoke (~ a few seconds, never disrupts the device beyond reboot)
    python tests/integration/test_gateway.py --url http://192.168.1.42

    # Skip the reboot test (no downtime)
    python tests/integration/test_gateway.py --url http://192.168.1.42 --skip-reboot

    # Full end-to-end with a real sensor — adds the sensor (if not already
    # there), waits up to 20 min for a LoRa packet, verifies the gateway
    # decoded it into physically sensible values:
    python tests/integration/test_gateway.py \\
        --url http://192.168.1.42 \\
        --test-sensor-sn 475481 \\
        --test-sensor-key D43C2780 \\
        --test-sensor-name integration-test \\
        --test-sensor-type 1

    # Useful flags:
    #   -v / --verbose      print every HTTP request URL
    #   --no-color          disable ANSI colours (for CI / log files)
    #   --skip-reboot       skip the reboot recovery test
    #   --packet-timeout N  override the 1200 s default wait for a packet

Exit codes:
    0   all tests passed
    1   at least one test failed
    2   could not connect to device
"""

import argparse
import json
import re
import sys
import time
from typing import Callable, List, Optional, Tuple

try:
    import requests
except ImportError:
    print("ERROR: this script needs 'requests'. Run:  pip install requests")
    sys.exit(2)

# --------------------------------------------------------------------- output

# Globally mutable so --no-color can blank them in one place
GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
DIM = "\033[2m"
RESET = "\033[0m"
BOLD = "\033[1m"


def disable_colors():
    global GREEN, RED, YELLOW, CYAN, DIM, RESET, BOLD
    GREEN = RED = YELLOW = CYAN = DIM = RESET = BOLD = ""


def pass_(name: str, detail: str = ""):
    msg = f"  {GREEN}[PASS]{RESET} {name}"
    if detail:
        msg += f"  {DIM}{detail}{RESET}"
    print(msg)


def fail_(name: str, detail: str = ""):
    msg = f"  {RED}[FAIL]{RESET} {name}"
    if detail:
        msg += f"  {DIM}{detail}{RESET}"
    print(msg)


def info_(line: str):
    print(f"{DIM}{line}{RESET}")


def section(title: str):
    print(f"\n{BOLD}{CYAN}== {title} =={RESET}")


# --------------------------------------------------------------- assertions

class TestFailed(Exception):
    pass


def assert_eq(expected, actual, label: str):
    if expected != actual:
        raise TestFailed(f"{label}: expected {expected!r}, got {actual!r}")


def assert_in(needle: str, haystack: str, label: str):
    if needle not in haystack:
        raise TestFailed(f"{label}: did not find {needle!r} in response")


def assert_status(resp, expected_codes: List[int]):
    if resp.status_code not in expected_codes:
        raise TestFailed(
            f"unexpected status {resp.status_code} (wanted one of {expected_codes})"
        )


# ------------------------------------------------------------------ runner

class Runner:
    def __init__(self, base_url: str, verbose: bool = False):
        self.base = base_url.rstrip("/")
        self.verbose = verbose
        self.failures: List[str] = []
        self.session = requests.Session()
        # short timeout — device on local network should respond fast
        self.session.headers.update({"User-Agent": "explora-integration-test/1.0"})

    def run(self, name: str, fn: Callable):
        try:
            detail = fn()
            pass_(name, detail or "")
        except TestFailed as e:
            fail_(name, str(e))
            self.failures.append(name)
        except requests.exceptions.RequestException as e:
            fail_(name, f"transport error: {e}")
            self.failures.append(name)
        except Exception as e:  # noqa: BLE001
            fail_(name, f"unexpected exception: {e!r}")
            self.failures.append(name)

    def get(self, path: str, **kwargs) -> requests.Response:
        url = self.base + path
        if self.verbose:
            info_(f"  GET {url}")
        return self.session.get(url, timeout=kwargs.pop("timeout", 5), **kwargs)

    def post(self, path: str, data=None, **kwargs) -> requests.Response:
        url = self.base + path
        if self.verbose:
            info_(f"  POST {url}  data={data}")
        return self.session.post(
            url, data=data, timeout=kwargs.pop("timeout", 5), **kwargs
        )


# -------------------------------------------------------------- read-only

def t_firmware_version(r: Runner) -> str:
    resp = r.get("/firmware/version")
    assert_status(resp, [200])
    body = resp.json()
    if "version" not in body:
        raise TestFailed(f"no 'version' field in JSON: {body}")
    ver = body["version"]
    if not re.match(r"^\d+\.\d+\.\d+$", str(ver)):
        raise TestFailed(f"version {ver!r} is not semver-shaped")
    return f"v{ver}"


def t_api_json(r: Runner) -> str:
    resp = r.get("/api?format=json")
    assert_status(resp, [200])
    body = resp.json()
    for key in ("version", "time", "status", "sensors"):
        if key not in body:
            raise TestFailed(f"missing '{key}' in /api JSON: keys={list(body.keys())}")
    if not isinstance(body["sensors"], list):
        raise TestFailed("'sensors' is not a JSON array")
    return f"{len(body['sensors'])} sensors, status={body['status']!r}"


def t_api_csv(r: Runner) -> str:
    resp = r.get("/api?format=csv")
    assert_status(resp, [200])
    ct = resp.headers.get("Content-Type", "")
    if "csv" not in ct and "text" not in ct:
        raise TestFailed(f"unexpected Content-Type for CSV: {ct!r}")
    lines = resp.text.strip().splitlines()
    if not lines:
        raise TestFailed("empty CSV response")
    return f"{len(lines)} line(s)"


def t_sensors_page(r: Runner) -> str:
    resp = r.get("/sensors")
    assert_status(resp, [200])
    html = resp.text
    assert_in("Configured Sensors", html, "sensors page heading")
    # Health columns added in 1.1.11 — regression guard
    assert_in("Status", html, "Status column header (added in 1.1.11)")
    assert_in("24h", html, "24h column header (added in 1.1.11)")
    return "OK + has new health columns"


def t_config_page(r: Runner) -> str:
    resp = r.get("/config")
    assert_status(resp, [200])
    html = resp.text.lower()
    assert_in("ssid", html, "ssid form field")
    assert_in("password", html, "password form field")
    return "OK"


def t_root(r: Runner) -> str:
    resp = r.get("/", allow_redirects=False)
    assert_status(resp, [200, 302, 303])
    return f"status={resp.status_code}"


def t_diag_heap(r: Runner) -> str:
    resp = r.get("/diag/heap")
    assert_status(resp, [200])
    # Don't assume JSON vs HTML — just look for the word "heap"
    if "heap" not in resp.text.lower():
        raise TestFailed("no 'heap' word in /diag/heap response")
    return "OK"


def t_logs_page(r: Runner) -> str:
    resp = r.get("/logs")
    assert_status(resp, [200])
    return "OK"


def t_unknown_url_returns_404(r: Runner) -> str:
    resp = r.get("/this/does/not/exist", allow_redirects=False)
    # 404 in STA mode, 302 to /config in AP captive mode — both acceptable
    if resp.status_code not in (404, 302, 303):
        raise TestFailed(f"unexpected status {resp.status_code}")
    return f"status={resp.status_code}"


def t_api_unknown_sensor(r: Runner) -> str:
    resp = r.get("/api?sensor=FFFFFF&format=json")
    # Accept any 2xx or 404 — both are sane semantics for "not found"
    if resp.status_code not in (200, 404):
        raise TestFailed(f"unexpected status {resp.status_code}")
    return f"status={resp.status_code}"


# --------------------------------------------------------- POST validation

def t_config_empty_ssid_rejected(r: Runner) -> str:
    resp = r.post("/config", data={"ssid": "", "password": "somepass"})
    assert_status(resp, [400])
    # Validation added in 1.1.6 — regression guard
    if "ssid" not in resp.text.lower() and "empty" not in resp.text.lower():
        raise TestFailed(f"400 returned but message unclear: {resp.text!r}")
    return "400 + clear message"


def t_config_short_password_rejected(r: Runner) -> str:
    resp = r.post("/config", data={"ssid": "test-ssid", "password": "1234"})
    assert_status(resp, [400])
    if "password" not in resp.text.lower():
        raise TestFailed(f"400 returned but message unclear: {resp.text!r}")
    return "400 + clear message"


def t_config_missing_params_rejected(r: Runner) -> str:
    resp = r.post("/config", data={"ssid": "only-ssid"})  # no password
    assert_status(resp, [400])
    return "400"


def t_config_too_long_ssid_rejected(r: Runner) -> str:
    resp = r.post(
        "/config", data={"ssid": "x" * 33, "password": "validpass123"}
    )
    assert_status(resp, [400])
    return "400 (>32 chars)"


# ---------------------------------------- live sensor (real LoRa packet)

def _find_sensor(api_body: dict, sn_hex: str) -> Optional[dict]:
    """Return the sensor dict from /api JSON whose serialNumber matches sn_hex (case-insensitive)."""
    want = sn_hex.upper().lstrip("0")
    for s in api_body.get("sensors", []):
        got = str(s.get("serialNumber", "")).upper().lstrip("0")
        if got == want:
            return s
    return None


def t_add_or_verify_test_sensor(
    r: Runner, sn: str, key: str, name: str, sensor_type: int
) -> str:
    """Add the test sensor if it's not yet configured. Idempotent across reruns."""
    # 1. existence check via /api
    resp = r.get(f"/api?sensor={sn}&format=json")
    assert_status(resp, [200])
    existing = _find_sensor(resp.json(), sn)
    if existing is not None:
        return f"sensor {sn} already configured (name={existing.get('name')!r})"

    # 2. POST add
    resp = r.post(
        "/sensors/add",
        data={
            "name": name,
            "deviceType": str(sensor_type),
            "serialNumber": sn,
            "deviceKey": key,
        },
    )
    # The handler responds 200 + HTML redirect on success
    if resp.status_code not in (200, 302, 303):
        raise TestFailed(f"add returned status {resp.status_code}: {resp.text[:120]!r}")

    # 3. verify visible in /api
    resp = r.get(f"/api?sensor={sn}&format=json")
    if _find_sensor(resp.json(), sn) is None:
        raise TestFailed(f"sensor {sn} not visible in /api after add")
    return f"sensor {sn} added as {name!r}"


def t_wait_for_packet(
    r: Runner, sn: str, timeout_s: int, poll_interval_s: int = 30
) -> str:
    """Poll /api until lastSeen indicates the gateway received a fresh packet.

    lastSeen is reported as "seconds since the packet arrived" (or -1 if the
    sensor has never been seen). We consider lastSeen < 2*poll_interval a
    fresh packet — within one poll window after it actually landed.
    """
    started = time.time()
    deadline = started + timeout_s
    fresh_threshold = poll_interval_s * 2
    info_(
        f"  polling /api every {poll_interval_s}s for up to {timeout_s}s "
        f"(fresh = lastSeen < {fresh_threshold}s)"
    )
    while time.time() < deadline:
        elapsed = int(time.time() - started)
        sensor = None
        ls_repr = "?"
        try:
            resp = r.get(f"/api?sensor={sn}&format=json", timeout=5)
            sensor = _find_sensor(resp.json(), sn)
            if sensor is not None:
                ls = sensor.get("lastSeen", -1)
                ls_repr = str(ls)
                if isinstance(ls, (int, float)) and 0 <= ls < fresh_threshold:
                    return (
                        f"packet received after {elapsed}s "
                        f"(lastSeen={ls}s, temp={sensor.get('temperature')}, "
                        f"hum={sensor.get('humidity')}, press={sensor.get('pressure')}, "
                        f"batt={sensor.get('batteryVoltage')}V)"
                    )
        except requests.exceptions.RequestException as e:
            ls_repr = f"req error: {e}"

        info_(f"    [t={elapsed:4d}s] lastSeen={ls_repr}")
        time.sleep(poll_interval_s)

    raise TestFailed(
        f"no fresh packet from sensor {sn} within {timeout_s}s — sensor offline, "
        f"out of range, or wrong key?"
    )


def t_validate_decoded_values(r: Runner, sn: str, sensor_type: int) -> str:
    """Verify the gateway decoded the received packet into physically plausible values.

    This is the end-to-end signal that decrypt + parse + calibration pipeline
    is working on real hardware (the native unit tests cover correctness in
    isolation; this one catches integration regressions).
    """
    resp = r.get(f"/api?sensor={sn}&format=json")
    sensor = _find_sensor(resp.json(), sn)
    if sensor is None:
        raise TestFailed(f"sensor {sn} not in /api response")

    issues: List[str] = []

    # Battery is reported for every sensor type
    batt = sensor.get("batteryVoltage", 0)
    if not (0.5 <= float(batt) <= 5.0):
        issues.append(f"batteryVoltage {batt} V out of (0.5, 5.0)")

    # Per-type range checks. Liberal bounds — we're catching "wildly wrong"
    # not "minor calibration drift".
    if sensor_type == 1:  # BME280
        temp = sensor.get("temperature", 0)
        if not (-40.0 <= float(temp) <= 80.0):
            issues.append(f"temperature {temp} °C outside (-40, 80)")
        hum = sensor.get("humidity", 0)
        if not (0.0 <= float(hum) <= 100.0):
            issues.append(f"humidity {hum}% outside (0, 100)")
        press = sensor.get("pressure", 0)
        if not (850.0 <= float(press) <= 1100.0):
            issues.append(f"pressure {press} hPa outside (850, 1100)")
    elif sensor_type == 2:  # SCD40
        temp = sensor.get("temperature", 0)
        if not (-40.0 <= float(temp) <= 80.0):
            issues.append(f"temperature {temp} °C outside (-40, 80)")
        ppm = sensor.get("ppm", 0)
        if not (300.0 <= float(ppm) <= 10000.0):
            issues.append(f"CO2 {ppm} ppm outside (300, 10000)")
    elif sensor_type == 4:  # VEML7700
        lux = sensor.get("lux", 0)
        if not (0.0 <= float(lux) <= 200000.0):
            issues.append(f"lux {lux} outside (0, 200k)")

    if issues:
        raise TestFailed("; ".join(issues))

    # Build a friendly summary based on what fields are populated
    parts = []
    for k in ("temperature", "humidity", "pressure", "ppm", "lux"):
        if k in sensor and sensor[k] != 0:
            parts.append(f"{k}={sensor[k]}")
    parts.append(f"batt={sensor.get('batteryVoltage')}V")
    parts.append(f"rssi={sensor.get('rssi')}dBm")
    return ", ".join(parts)


def t_sensor_visible_in_sensors_page(r: Runner, sn: str) -> str:
    """Verify the sensor's SN shows up on the /sensors HTML page."""
    resp = r.get("/sensors")
    assert_status(resp, [200])
    sn_upper = sn.upper().lstrip("0")
    # The HTML uses formatSN which uppercases and zero-pads to 6 chars
    if sn_upper not in resp.text.upper():
        raise TestFailed(f"SN {sn_upper} not present in /sensors HTML")
    return f"SN {sn_upper} visible on /sensors page"


# --------------------------------------------------------------- reboot

def t_reboot_round_trip(r: Runner, current_version: str) -> str:
    """Best-effort reboot smoke test.

    POST /reboot, wait for the device to drop off, poll until it comes
    back, then verify it's serving requests again with the same version.
    """
    # 1) snapshot pre-reboot
    info_(f"  pre-reboot version: {current_version}")

    # 2) issue reboot. The endpoint may not return 200 (device restarts
    # mid-response), so we tolerate connection errors here.
    try:
        resp = r.get("/reboot", timeout=3)
        info_(f"  /reboot returned {resp.status_code}")
    except requests.exceptions.RequestException as e:
        info_(f"  /reboot failed (expected — device rebooting): {e}")

    # 3) wait for device to go down (it might already be down)
    info_("  waiting up to 10 s for device to drop off …")
    deadline = time.time() + 10
    went_down = False
    while time.time() < deadline:
        try:
            r.get("/firmware/version", timeout=1)
        except requests.exceptions.RequestException:
            went_down = True
            break
        time.sleep(0.5)
    if not went_down:
        # Device didn't reboot — endpoint stayed up the whole time.
        raise TestFailed("/reboot did not cause the device to drop off (endpoint stayed responsive)")

    # 4) wait up to 60 s for device to come back
    info_("  waiting up to 60 s for device to come back …")
    deadline = time.time() + 60
    while time.time() < deadline:
        try:
            resp = r.get("/firmware/version", timeout=2)
            if resp.status_code == 200:
                body = resp.json()
                returned = body.get("version")
                if returned == current_version:
                    return f"back after reboot, version={returned}"
                else:
                    raise TestFailed(
                        f"version changed across reboot: pre={current_version} post={returned}"
                    )
        except requests.exceptions.RequestException:
            pass
        time.sleep(1)
    raise TestFailed("device did not come back within 60 s")


# ----------------------------------------------------------------- main

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url",
        required=True,
        help="Base URL of the gateway (e.g. http://192.168.1.42 or http://gateway.local)",
    )
    parser.add_argument(
        "--skip-reboot",
        action="store_true",
        help="Skip the reboot smoke test (the only test that disrupts the device)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Print each request URL"
    )
    parser.add_argument(
        "--no-color", action="store_true", help="Disable ANSI colour escape sequences"
    )
    # Live-sensor round-trip: only runs when --test-sensor-sn is provided.
    parser.add_argument(
        "--test-sensor-sn",
        help="Hex serial number of a real sensor to register and wait for "
        "(e.g. 475481). Enables the live LoRa packet test.",
    )
    parser.add_argument(
        "--test-sensor-key",
        help="Hex device key for --test-sensor-sn (e.g. D43C2780). Required if "
        "--test-sensor-sn is given.",
    )
    parser.add_argument(
        "--test-sensor-name",
        default="integration-test",
        help="Display name to give the test sensor (default: integration-test)",
    )
    parser.add_argument(
        "--test-sensor-type",
        type=int,
        default=1,
        help="Sensor type code: 1=BME280, 2=SCD40, 3=METEO, 4=VEML7700, "
        "0x51=DIY_TEMP (default: 1)",
    )
    parser.add_argument(
        "--packet-timeout",
        type=int,
        default=1200,
        help="Seconds to wait for the first LoRa packet (default: 1200 = 20 min)",
    )
    args = parser.parse_args()

    if args.no_color:
        disable_colors()

    if args.test_sensor_sn and not args.test_sensor_key:
        print(f"{RED}ERROR:{RESET} --test-sensor-key is required when --test-sensor-sn is given")
        return 2

    r = Runner(args.url, verbose=args.verbose)

    # Sanity ping — fail fast with a clear message if device isn't reachable
    try:
        resp = r.get("/firmware/version", timeout=3)
        body = resp.json()
        current_version = body.get("version", "unknown")
    except Exception as e:  # noqa: BLE001
        print(f"{RED}ERROR:{RESET} cannot reach {args.url}/firmware/version  ({e})")
        print("Is the device powered on and on the same network?")
        return 2

    print(f"{BOLD}Testing gateway at {args.url}  (firmware v{current_version}){RESET}")

    section("Read-only smoke")
    r.run("firmware version JSON", lambda: t_firmware_version(r))
    r.run("/api?format=json shape", lambda: t_api_json(r))
    r.run("/api?format=csv basics", lambda: t_api_csv(r))
    r.run("/sensors page (has new health columns)", lambda: t_sensors_page(r))
    r.run("/config form", lambda: t_config_page(r))
    r.run("/ (root)", lambda: t_root(r))
    r.run("/diag/heap", lambda: t_diag_heap(r))
    r.run("/logs", lambda: t_logs_page(r))
    r.run("unknown URL → 404", lambda: t_unknown_url_returns_404(r))
    r.run("/api with unknown sensor", lambda: t_api_unknown_sensor(r))

    section("POST validation (added in 1.1.6)")
    r.run("empty SSID → 400", lambda: t_config_empty_ssid_rejected(r))
    r.run("password < 8 chars → 400", lambda: t_config_short_password_rejected(r))
    r.run("missing password param → 400", lambda: t_config_missing_params_rejected(r))
    r.run("SSID > 32 chars → 400", lambda: t_config_too_long_ssid_rejected(r))

    if args.test_sensor_sn:
        section(
            f"Live sensor — packet round trip (SN {args.test_sensor_sn}, type "
            f"{args.test_sensor_type}, timeout {args.packet_timeout}s)"
        )
        r.run(
            "add or verify test sensor",
            lambda: t_add_or_verify_test_sensor(
                r,
                args.test_sensor_sn,
                args.test_sensor_key,
                args.test_sensor_name,
                args.test_sensor_type,
            ),
        )
        r.run(
            f"wait for fresh LoRa packet (up to {args.packet_timeout}s)",
            lambda: t_wait_for_packet(r, args.test_sensor_sn, args.packet_timeout),
        )
        r.run(
            "decoded values are physically plausible",
            lambda: t_validate_decoded_values(
                r, args.test_sensor_sn, args.test_sensor_type
            ),
        )
        r.run(
            "sensor visible on /sensors page",
            lambda: t_sensor_visible_in_sensors_page(r, args.test_sensor_sn),
        )
    else:
        info_("\nSkipping live sensor test (--test-sensor-sn not provided)")

    if args.skip_reboot:
        info_("Skipping reboot test (--skip-reboot)")
    else:
        section("Reboot recovery")
        r.run("device reboots and returns", lambda: t_reboot_round_trip(r, current_version))

    # ------------------------------------------------------------- summary
    print()
    if r.failures:
        print(f"{RED}{BOLD}FAILED:{RESET} {len(r.failures)} test(s)")
        for f in r.failures:
            print(f"  - {f}")
        return 1
    else:
        print(f"{GREEN}{BOLD}ALL PASSED{RESET}")
        return 0


if __name__ == "__main__":
    sys.exit(main())

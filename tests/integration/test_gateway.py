#!/usr/bin/env python3
"""
Integration smoke test for a running expLORA Gateway Lite device.

Hits the device over HTTP and asserts response status, content type,
JSON shape, validation behaviour, and (optionally) reboot recovery.

Requirements:
    pip install requests

Usage:
    python tests/integration/test_gateway.py --url http://192.168.1.42
    python tests/integration/test_gateway.py --url http://gateway.local --skip-reboot
    python tests/integration/test_gateway.py --url http://192.168.1.42 --verbose

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

GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
DIM = "\033[2m"
RESET = "\033[0m"
BOLD = "\033[1m"


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
    args = parser.parse_args()

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

    if args.skip_reboot:
        info_("\nSkipping reboot test (--skip-reboot)")
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

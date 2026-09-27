"""USB-only BLE computer pairing management. Requires: python -m pip install hidapi."""
import argparse
import json
import time
import hid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--forget-host', action='store_true', help='Clear only the BLE computer bond and open pairing for 120 seconds')
parser.add_argument('--serial', help='Select a specific StopWatch when several are connected')
args = parser.parse_args()
devices = [d for d in hid.enumerate() if d.get('product_string') == 'StopWatch Dongle (experimental)'
           and d.get('usage_page') == 0xff50
           and (not args.serial or d.get('serial_number') == args.serial)]
if len(devices) != 1:
    raise SystemExit(f'Expected one USB StopWatch vendor HID interface; found {len(devices)}. Connect USB; use --serial if necessary.')
device = hid.device()
try:
    device.open_path(devices[0]['path'])
    def status():
        raw = bytes(device.get_feature_report(7, 64))
        if len(raw) != 64 or raw[:6] != b'\x07SWBT\x01':
            raise RuntimeError('Firmware does not support BLE host management')
        return {'paired': bool(raw[6]), 'ready': bool(raw[7]),
                'reset': {0: 'idle', 1: 'pending', 2: 'done', 3: 'error'}.get(raw[8], 'unknown')}
    current = status()
    if args.forget_host:
        device.send_feature_report(b'\x07SWBT\x01\x01\x00\x00' + bytes(55))
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            time.sleep(0.1)
            current = status()
            if current['reset'] in ('done', 'error'):
                break
        if current['reset'] != 'done' or current['paired']:
            raise RuntimeError(f'Computer bond reset failed or timed out: {current}')
    print(json.dumps(current, indent=2))
finally:
    device.close()

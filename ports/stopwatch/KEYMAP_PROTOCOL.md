# StopWatch keymap protocol v1

This is a project-specific WebHID protocol, not ZMK Studio. Usage page `0xff50`,
usage `1`, top-level vendor collection in the existing HID interface. Feature
report ID `5`: exactly 63 payload bytes (64 including the report ID).

Request bytes: `S K 1 command sequence:u32le payload[55]`.
Response: same first 8 bytes, then `result:u8 payload[54]`. Poll GET_REPORT until
command and sequence match. A successful SET_REPORT only queues a request; it
does not acknowledge saving. One in-flight request per client; other clients may
receive a conflict, busy result or timeout. No unsolicited input reports.

| Command | Request payload | Response payload |
|---|---|---|
| 1 INFO | empty | layers:u8, keys:u8, binding-size:u8, chunk:u8, schema:u32, revision:u32, saved:u8 |
| 2 READ | offset:u16, count:u8, defaults:u8 | offset:u16, count:u8, bytes[count] |
| 3 BEGIN | expected-revision:u32, schema:u32 | token:u32 (request sequence) |
| 4 WRITE | token:u32, offset:u16, count:u8, bytes[count] | empty |
| 5 COMMIT | token:u32, FNV-1a:u32 | new-revision:u32 |
| 6 ABORT | token:u32 | empty |

All integers little-endian. Maximum chunk: 48 bytes. Full map: 1500 bytes,
layer-major, 5 × 50 × 6. Each binding is `kind:u8, modifiers:u8, code:u16,
usage-page:u8, layer:u8`; it is explicitly serialized, not a native C struct.
Kinds match `engine.h`: none, transparent, key, layer-tap, compiled macro.

WRITE must be sequential and cover the entire map. BEGIN checks the current
revision, rejects simultaneous transactions, and issues a transaction token.
Staging expires after 30 seconds without a write, or on USB disconnect/suspend.
COMMIT validates length, checksum and every binding; refuses while a key is held;
saves one NVS blob and commits it before changing the running keymap. Invalid or
partial writes never modify the active map. A new BEGIN is needed after expiry.
On response loss, read back before deciding whether to retry a save.

Results: 0 OK, 1 malformed, 2 incompatible schema, 3 stale revision/token,
4 busy/held key, 5 wrong offset, 6 invalid map/checksum, 7 storage failed,
8 expired/unknown transaction.

Persistence: NVS namespace `sw_keymap`, key `map`: `SKM\x01`, schema:u32,
FNV-1a:u32, map[1500]. Exact size, schema, checksum and binding validity are all
checked at boot. Invalid/incompatible storage falls back to compiled defaults;
no NVS partition or Bluetooth bond erase is performed. Schema derives from
protocol version, layer count and compiled macro names/definitions, so changing
only the default key bindings does not discard a user's saved layout.

# Release tracing on both halves

Build `corne-left-diagnostic` and `corne-right-diagnostic` as described in the
[README](../README.md). These use the same pin, keymap, display, +8 dBm and debounce
as normal firmware. They add upstream debug logs, read-only observers, a separate
CDC console, and a shell. No retry, heartbeat, connection tuning, ADC resampling,
bond/profile changes or automatic endpoint selection are added.

## Capture

1. Flash both diagnostic images. Connect each half's USB cable and open its serial
   console at 115200 baud using a terminal such as `tio` or `picocom`. On Linux,
   inspect `/dev/serial/by-id/` and `/dev/ttyACM*`; on macOS, `/dev/cu.usbmodem*`.
   Open one half at a time initially to identify ports. The left has separate
   **Studio and logging CDC ports**; the logging port presents the shell prompt.
   Do not assume ACM numbering. Keep Studio disconnected during timing tests.
2. In each console run `corne status`. It identifies `half=left` or `half=right`,
   cached battery mV/SOC, and live connection parameters. Save full console output
   separately as `left.log` and `right.log`, including a status snapshot.
3. In the **left** console run `corne output ble`, then `corne status`. Confirm
   `Selected endpoint=2`. USB logging otherwise makes USB HID preferred and can
   hide the host-BLE problem. If BLE disconnects, upstream may fall back to USB;
   inspect endpoint logs and take another status snapshot after an incident.
4. On Linux capture `sudo btmon -w corne.btsnoop` and `sudo evtest /dev/input/eventN`
   for the Corne device in separate terminals. Record `journalctl -k`/Bluetooth
   service messages around disconnects. `evtest` values are 1=press, 0=release,
   2=OS repeat. Use the saved HCI trace to inspect HID notifications and connection
   updates, and the input trace to establish when the kernel saw the release.
5. Type a deliberate identifiable pattern and hold/release a key from each half.
   Capture complete sequences around a repeat. Log test text only: key events and
   HID payloads expose what was typed.
6. Finish with `corne output usb` on the left if returning to the default preferred
   transport, wait at least one second for settings persistence, then flash both
   normal images. `corne output` explicitly saves the transport preference; it
   does not alter bonding. The other diagnostic commands are observational.

Logging changes scheduling/power and USB charges the batteries. A fault disappearing
with logging attached does not prove it was fixed. Compare normal battery-only
behavior too. Watch for dropped-log messages: absence in an incomplete log cannot
prove an event never happened. Hardware timing and runtime stack usage still need
device testing; extra stack/log buffer space is confined to diagnostic builds.

## Evidence at each stage

| Stage | Where to look | What it establishes |
| --- | --- | --- |
| Debounced physical scan | Local `kscan_matrix_read: Sending event at r,c state off` | Matrix driver detected a release after debounce, before its callback. This is not raw electrical switch sampling. |
| Matrix queue | `QUEUE_PUT_FAILED queue=matrix` | The observed scan could not enter the position-processing queue. Existing return behavior is preserved. |
| Position event | `POSITION source=255 position=N state=0 event_ms=… now_ms=…` | Local position event raised before listeners can consume it. Also upstream row/column-to-position log. |
| Right TX queue | Right `QUEUE_PUT_FAILED queue=split_tx`, existing queue-full messages | Failure/stall while queueing full position state. |
| Right BLE submission | Right `NOTIFY_BEGIN`, full bitmap payload, matching `NOTIFY_END seq=N rc=…` | A state bitmap was submitted; zero return means accepted by Zephyr. `conn=-1` means notify subscribed connections, not a missing connection. |
| Left BLE receipt | Left `split_central_notify_func: [NOTIFICATION]` plus `data` hexdump | The central's subscription callback received the full position bitmap. |
| Central queue/event | `QUEUE_PUT_FAILED queue=split_rx`; left `POSITION source=0 position=N state=0` | The right release passed the RX queue and became a central event. `source=0` is this setup's only peripheral. |
| Keymap/HID generation | `KEYCODE … state=0`, upstream `hid_listener_keycode_released`, `left HID_ENQUEUE body` | Keymap release reached HID report construction and the resulting report entered the send path. |
| Host TX queue | `HID_ENQUEUE rc=…`, `QUEUE_PUT_FAILED queue=hid_tx`, existing HOG queue warnings | Queue acceptance or failure, distinct from later notification. |
| Host BLE submission | Left `NOTIFY_BEGIN` payload and matching `NOTIFY_END rc=…` | The queued HID report reached the BLE stack. Use attribute handle and payload length to distinguish HID from Studio/other notifications. |
| Host receipt/input | `btmon` HID notification, then `evtest` release | Evidence at the host controller/stack and input layer. Firmware submission success alone cannot establish this. |

Split payload bit `position % 8` in byte `position / 8` is 1=held, 0=released.
Compare full bitmaps on both halves. HID payloads are the resolved build's report
body; use its descriptor/ELF and the HCI trace rather than assuming boot-report
layout. The logged attribute may be a characteristic declaration; its wire value
handle is then the following handle. Notifications retain original callbacks and
return values; no fake delivery acknowledgment is added.

The GPIO driver and central subscription callback are upstream debug logs. The
optional module uses GNU link wrappers around external event dispatch, GATT notify,
HID enqueue, and relevant Zephyr queue writes. It forwards the original arguments
and return values exactly. Normal firmware does not compile these observers.

## Connections and battery

Connection logs label half, link (`host` or `split`), connection index, security,
connect error, disconnect reason, pairing completion/failure and PHY updates.
`interval_units * 1.25` gives milliseconds; `timeout_units * 10` gives milliseconds;
latency is the allowed skipped connection-event count. PHY values are Zephyr's
enumeration (1=1M, 2=2M, 4=Coded). `corne status` shows initial/current values even
if no parameter-update callback occurred. PHY support is enabled for observation;
the module never calls the PHY or connection-parameter update APIs.

Each port's upstream `vddh_sample_fetch: ADC raw … ~ … mV => …%` is the **local**
battery. The half identity is printed by `corne status`. `cached_mv` and
`cached_soc` use the last upstream sample, which can be stale while idle or zero
before the first sample; the command does not trigger an ADC read. No right-side
voltage is transmitted over the normal split protocol. Capture the right port to
see right voltage. Compare the ADC log timestamp to the incident and note USB power.

## Timestamp interpretation

Each half and the host have independent clocks. Firmware log timestamps are local
uptime, not wall time. A position event's timestamp is generated when its worker
processes it, not at electrical release; a received right-side event gets a central
timestamp. You may subtract timestamps **within one device** to locate queue or
processing gaps. Match distinctive event patterns/bitmaps across devices, but do
not subtract their raw clocks to claim end-to-end BLE latency. USB serial buffering
also makes host receipt time of a log line different from firmware event time.

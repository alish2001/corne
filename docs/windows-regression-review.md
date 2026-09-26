# Windows/Intel regression review — 2026-09-25

A requested research subagent reviewed upstream ZMK/Zephyr issues, and the primary
agent cross-checked the strongest findings against the exact source pins and saved
build configurations. **A concrete compatibility mechanism was found; the cause
of this keyboard's failure is not yet confirmed.** No Windows pairing trace or
test result for the optional compatibility image is available yet.

Old pins: ZMK `edf5c0814fd3ea202e43aad2d68fd32e882a518c` (v0.3), Zephyr
`dacab4875df72109b96cc8977547a0dc04875bcd` (3.5).
New pins: ZMK `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0`, Zephyr
`10ba6d0cb38bc3d258775d27982f707599320085` (4.1).

## Strongest lead: connection negotiation collisions

[Zephyr #71249](https://github.com/zephyrproject-rtos/zephyr/issues/71249), reported
April 9, 2024, describes a Windows host starting a connection-parameter procedure
before an earlier radio-speed update finished. [PR #71221](https://github.com/zephyrproject-rtos/zephyr/pull/71221),
merged April 11, changed the controller to disconnect for that invalid overlap,
as required by the Bluetooth specification.

The relevant commit, `ad46ed78d4896eb54564fe90872feb3a5752afeb`, is an ancestor of
our new Zephyr pin. The old pinned `ull_llcp_phy.c` does not set the reserved
incompatibility flag upon receiving the update indication; the
[new pinned code does](https://github.com/zmkfirmware/zephyr/blob/10ba6d0cb38bc3d258775d27982f707599320085/subsys/bluetooth/controller/ll_sw/ull_llcp_phy.c#L716).
The new controller's collision path can terminate with **0x23** (same procedure)
or **0x2A** (different procedures). The old/new source difference was verified
directly, independently of commit ancestry.

[Zephyr #81088](https://github.com/zephyrproject-rtos/zephyr/pull/81088), opened
November 7, 2024 and closed **unmerged** January 28, 2025, provides concrete Intel
corroboration: an nRF5340 peripheral disconnected from an Intel Meteor Lake
Bluetooth 5.4 host with reason 0x23. A maintainer's
[radio-trace analysis](https://github.com/zephyrproject-rtos/zephyr/pull/81088#issuecomment-2470909617)
confirmed overlapping radio-speed changes. The reporter
[confirmed that disabling automatic PHY updates worked](https://github.com/zephyrproject-rtos/zephyr/pull/81088#issuecomment-2472997852).
The proposed permissive-controller patch was not merged.

This is a plausible mechanism for “old firmware works, new firmware fails only
with Windows/Intel,” including on capable hardware. The reported chip combination
differs from this Corne, and we have no failure reason from this user's controller.
It therefore remains a candidate, not a diagnosed fault. Both our old and new
builds permit 2M PHY and automatic PHY updates; the surrounding negotiation code
changed between them.

## Open ZMK reports worth following

| Report | Status at review | What it contributes | Limit |
| --- | --- | --- | --- |
| [#805](https://github.com/zmkfirmware/zmk/issues/805), Bluetooth security failing on Windows | Open; opened May 24, 2021 | Exact Windows retry message, nice!nano/Corne reports, and some phones/Macs working; later Intel-driver and 2M-workaround reports | Historical collection of several causes, not one demonstrated regression in this pin. Old driver versions mentioned there are not installation recommendations. |
| [#3158](https://github.com/zmkfirmware/zmk/issues/3158), v0.3 nice!nano Bluetooth issue | Open; opened December 22, 2025 | Reporter said v0.2 worked, v0.3 failed; disabling 2M helped | Predates this v0.3-to-main migration. Supports a compatibility test, not attribution to our exact new version. |
| [#2026](https://github.com/zmkfirmware/zmk/issues/2026), Ubuntu/Windows pairing on one adapter | Open; opened November 17, 2023 | Same hardware identity can retain conflicting host keys across operating systems | User already cleared the Omarchy slot, so do not assume this explains the current failure. No hidden Windows Corne entry was found. |

## Secondary source-confirmed change: minimum key length

The actual build configurations differ in `CONFIG_BT_SMP_MIN_ENC_KEY_SIZE`:
**7 bytes → 16 bytes**. [Zephyr #73217](https://github.com/zephyrproject-rtos/zephyr/pull/73217)
merged this default change on December 4, 2024. The new pin's
[pairing-request handler](https://github.com/zmkfirmware/zephyr/blob/10ba6d0cb38bc3d258775d27982f707599320085/subsys/bluetooth/host/smp.c#L2953)
rejects a request offering a smaller maximum key size.

`BT_SMP_SC_PAIR_ONLY=y` in both builds disables legacy pairing; it is not the same
as `BT_SMP_SC_ONLY`, which requires authenticated level-4 security. Thus the key
length change cannot be dismissed solely from the SC_PAIR_ONLY flag. However,
there is no evidence that this Windows machine offers less than 16 bytes.
An SMP request showing `max_key_size 0x10` rules this particular condition out;
SMP failure 0x06 with a smaller size supports it. **No security settings were
weakened or changed.** SMP error codes and HCI disconnect reasons are separate
namespaces and must not be interpreted interchangeably.

## Changes inspected without a matching failure

- ZMK `app/src/hog.c` is byte-for-byte identical between the selected revisions.
- ZMK `app/src/ble.c` pairing/bond acceptance logic is unchanged. Its differences
  are advertising/UUID/appearance macro updates. The new `BT_LE_ADV_OPT_CONN`
  still represents the same two option bits, and resolved appearance stays 961.
- `BT_CTLR_PERIPHERAL_RESERVE_MAX` became enabled. The related
  [#84212 fix](https://github.com/zephyrproject-rtos/zephyr/pull/84212) is already in
  the new pin; no matching Intel pairing regression was found for this setting.
- ATT/L2CAP buffer and receive-work-queue configuration changed with Zephyr 4.1.
  Some symbols were renamed/reorganized, so absent old symbols are not proof a
  feature was disabled. No observed queue errors currently tie these to pairing.

## Reports that should not drive a speculative fix

- [ZMK #3244](https://github.com/zmkfirmware/zmk/issues/3244) initially blamed the
  new endpoint logic but was resolved by adding the missing `//zmk` board variant.
  Our exact V2/ZMK variant and NVS are verified in generated configuration.
- [ZMK #3482](https://github.com/zmkfirmware/zmk/pull/3482) is closed **unmerged**;
  its proposed address-lookup change was based on a wrong-board/settings problem.
- [ZMK #3309](https://github.com/zmkfirmware/zmk/issues/3309) is open but concerns
  sleep/resume and GATT failures originally on v0.3, not this initial-pairing case.
- [Zephyr #109217](https://github.com/zephyrproject-rtos/zephyr/issues/109217) is open
  and includes a Windows/iPhone difference, but uses nRF54L15/Zephyr 4.2.1 and a
  post-encryption/MTU stall. It is not a direct match for nRF52840/Zephyr 4.1.
- [ZMK #3385](https://github.com/zmkfirmware/zmk/pull/3385) is an open automatic
  unpairing proposal. Applying it would change bond policy without proving cause.
- The nice!view blank output icon is independently explained by an unhandled
  `ZMK_TRANSPORT_NONE` case. It does not establish an advertising/security failure.

## Next evidence to collect

1. Capture one failed Windows attempt using the original left diagnostic image.
   Enable `kernel log_level zmk 4` and `kernel log_level corne_diag 3`; record
   connection, security/pairing, profile rejection and disconnect messages.
2. **0x23/0x2A** makes the negotiation-collision candidate much stronger. The
   already-built, single-setting compatibility image in
   [windows-pairing.md](windows-pairing.md) disables 2 Mbps capability. A successful
   comparison would show that this change helps, without proving the precise
   packet sequence. Turning off automatic PHY updates while retaining 2M support
   is a narrower possible follow-up; do not combine both experiments initially.
3. A security/missing-key error warrants inspecting retained keyboard/host bonds;
   a short-key rejection warrants inspecting the SMP request. Enable SMP debug
   only if needed and avoid sharing key material.
4. If attribution remains unclear, compare old/new firmware on the same current
   Windows driver with comparable pairing state. Previous success is relevant
   evidence but is not the same as a current controlled rollback result.

No upstream patch was applied and no additional radio/security settings were
changed during this review. The Windows compatibility image is built and checked,
but no successful hardware pairing result has been reported yet.

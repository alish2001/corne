# Gaming layout

Implemented in `config/corne.keymap` after layout approval. The four existing
layers retain their bindings; the three reserved slots now contain Gaming,
Game Fn and Modes.

## Play layer

Preserve the entire right-hand **finger-key** arrangement from the existing Base
layer, including all punctuation. In particular I/O/P stay on the top row, J/K
stay on the middle row, and M stays on the bottom row. Right thumbs are
Alt / Enter / Exit.

```text
LEFT                                      RIGHT
Esc   Q     W     E     R     T             Y     U     I     O     P    Bksp
Tab   A     S     D     F     G             H     J     K     L     ;      '
Ctrl  Z     X     C     V     B             N     M     ,     .     /    RShift
                  Fn   Shift Space         Alt  Enter  Exit
```

The left plays as previously proposed: WASD movement, Shift sprint, Space jump,
Ctrl dodge/dash and C crouch (match in-game bindings). Fn is a pure momentary
utility-layer key, with no tap/hold decision. Game actions depend on Cyberpunk's
current bindings; the firmware sends ordinary keys rather than action macros.

## Held Fn layer

Keep WASD, Ctrl, Shift and Space stable while Fn is held. Weapon numbers remain
on the left without taking the mouse hand away. The right becomes a number/menu
utility area; its arrow positions match the existing Raise layer's H/J/K/L row.
Hold the left Fn thumb, then hold physical F for the weapon wheel: this sends
Left Alt without F. The right Alt thumb is a duplicate of that modifier.

```text
LEFT                                      RIGHT
Exit  1     W     2     3     4             1     2     3     4     5     F5
Caps  A     S     D     Alt   H             Left  Down  Up    Right F1    Delete
Ctrl  O     M     J     N     P             6     7     8     9     0     F9
                  Fn   Shift Space         Alt  Enter  Exit
```

## Locked-mode selection

From normal typing, hold both existing Lower and Raise keys to reveal a temporary
Modes layer, then tap:

- **G**: lock into Gaming.
- **N**: lock into the existing Numpad layer.
- **Esc**: return to normal Base typing.

Use ZMK's [conditional layers](https://zmk.dev/docs/keymaps/conditional-layers),
with `if-layers = <1 2>` activating the Modes layer, and `&to` bindings selecting
Base/Numpad/Gaming. This gives time to hold the two thumbs and then choose the
mode, rather than requiring three key presses inside a combo timeout.

Layer IDs, retaining the four existing layers:

| ID | Layer | Activation |
| --- | --- | --- |
| 0 | Base | Default / return target |
| 1 | Lower | Existing momentary key |
| 2 | Raise | Existing momentary key |
| 3 | Numpad | Lower + Raise + N |
| 4 | Gaming | Lower + Raise + G |
| 5 | Game Fn | Gaming outer left thumb, held |
| 6 | Modes | Conditional on Lower + Raise |

This uses all three formerly reserved slots. Unassigned Modes keys emit
nothing, so this menu cannot accidentally reach the Raise layer's BT_CLR action.
ZMK routes releases using the layer state captured at each key's press, so the
two held layer keys release their original momentary-layer behaviors even after
selecting a locked mode. Release both thumbs before starting to play.

In Gaming, the former Lower key is Shift, so the normal Lower+Raise entry chord
is not active there. **Fn+Esc** or the right outer **Exit** thumb returns to Base;
then the user can select another locked mode. The existing Numpad layer retains
access to the normal Lower/ Raise keys through its transparent thumb bindings.
Power cycling returns to Base typing.

## Flashing and Studio

Use the normal left and right images from the same gaming build. Connect each
half over USB, double-tap its reset button to open its bootloader drive, and copy
the matching `.uf2` to that drive. It will reboot automatically. This update
keeps the working ZMK/Zephyr pins, +8 dBm, normal Bluetooth settings and 5/5 ms
debounce. It does not include the retired Windows compatibility experiment.

If you have previously saved changes in ZMK Studio, record those custom bindings
before proceeding. Saved Studio settings can override the compiled keymap and
hide the new layers. After flashing, connect the left half to
[ZMK Studio](https://zmk.studio/) and use **Restore Stock Settings** to load this
firmware's layout, then reapply any custom edits you recorded. See the
[upstream explanation](https://zmk.dev/docs/features/studio#keymap-changes).
Keep the layer order shown above so Modes has priority over Lower and Raise.
The separate settings-reset UF2 is not needed for this layout update.

## First-use check

- From Base, hold Lower + Raise and tap G; release both thumbs. The display
  should show Gaming. Verify the former Enter thumb now jumps (Space) and the
  former Lower thumb sends Shift.
- Hold the outer left Fn thumb: Q/E/R/T should send 1/2/3/4, F should hold Alt,
  and WASD/Shift/Space should remain available. Release all keys and verify
  movement and modifiers stop normally.
- Fn + Esc, or the outer right thumb, returns to Base. Check normal typing.
- Lower + Raise + N enters Numpad. From Numpad, Lower + Raise + Esc returns
  to Base. Lower + Raise + G should also switch directly from Numpad to Gaming.

The build and compiled configuration can be checked before flashing; physical
key behavior and in-game bindings still need this first-use check on the keyboard.

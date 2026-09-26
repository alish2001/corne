# Gaming layout draft — awaiting finalization

This document and the conversation diagram are a proposal. `config/corne.keymap`
is still unchanged; no gaming firmware has been built or flashed. The next firmware
update will be made after the user finalizes the layout.

## Play layer

Preserve the entire right-hand **finger-key** arrangement from the existing Base
layer, including all punctuation. In particular I/O/P stay on the top row, J/K
stay on the middle row, and M stays on the bottom row. Right thumbs retain the
previous gaming proposal's Alt / Enter / Exit arrangement pending review.

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

Planned layer IDs, retaining the four existing layers:

| ID | Layer | Activation |
| --- | --- | --- |
| 0 | Base | Default / return target |
| 1 | Lower | Existing momentary key |
| 2 | Raise | Existing momentary key |
| 3 | Numpad | Lower + Raise + N |
| 4 | Gaming | Lower + Raise + G |
| 5 | Game Fn | Gaming outer left thumb, held |
| 6 | Modes | Conditional on Lower + Raise |

This uses all three currently reserved slots. Unassigned Modes keys should emit
nothing, so this menu cannot accidentally reach the Raise layer's BT_CLR action.
Keep release handling for the two layer keys intact.

In Gaming, the former Lower key is Shift, so the normal Lower+Raise entry chord
is not active there. **Fn+Esc** or the right outer **Exit** thumb returns to Base;
then the user can select another locked mode. The existing Numpad layer retains
access to the normal Lower/ Raise keys through its transparent thumb bindings.
No persistent boot-mode change is proposed.

## Implementation boundary

After finalization: edit the keymap, build normal left/right plus relevant
diagnostic targets, verify the layer transitions, and publish the matching UF2s.
Keep the currently working ZMK/Zephyr pins, +8 dBm, normal Bluetooth settings and
5/5 ms debounce. Do not reintroduce the archived Windows compatibility setting.
If saved Studio bindings exist, coordinate their preservation/application rather
than assuming a newly compiled keymap automatically overrides them.

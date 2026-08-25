# M6 Mouse and Touchpad Controls

Northstar Settings exposes pointer controls only when the active Wayfire
session reports an enabled pointer through `input/list-devices`. Mouse and
touchpad controls are separated using the compositor-reported device names;
the accepted Intel laptop reports both a `Mouse` and a `TouchPad` device.

The first slice provides:

- independent mouse and touchpad cursor speed;
- independent natural-scrolling toggles;
- tap to click;
- disable touchpad while typing; and
- automatic, finger-count, or button-area touchpad clicking.

Values are written atomically to the user's `[input]` section in
`~/.config/wayfire.ini`. The writer accepts only the fixed Wayfire option
allowlist, preserves unrelated sections and comments, and reads the saved value
back before reporting success. Wayfire owns live application of those options;
Northstar does not add a privileged `/dev/input` reader.

## Focused gates

Run:

```sh
cmake --build build --parallel 2
env QT_QPA_PLATFORM=offscreen ctest --test-dir build \
  -R 'northstar-(inputcontroller|desktopsettings|shell-qml)' --output-on-failure
```

Before merge, the physical Intel acceptance must confirm that mouse and
touchpad speed, natural scrolling, tap to click, disable-while-typing, and the
supported click methods apply from Settings and survive logout or reboot.
Unavailable hardware must remain visibly unavailable rather than presenting a
dead control.

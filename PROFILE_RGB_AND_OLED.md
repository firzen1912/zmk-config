# Helix Bluetooth-profile RGB and OLED battery status

This config targets a Helix split keyboard using two nice!nano v2 controllers.

## Bluetooth profile colors

The central half listens for ZMK's active Bluetooth-profile change event and
invokes the global RGB underglow behavior. Both halves therefore use the same
profile color and connection-state pattern:

- **Connected:** solid profile color
- **Disconnected, unpaired, or waiting/advertising:** slow fading/breathing in
  the same profile color

| Profile | Color | HSB |
|---:|---|---:|
| 0 | White | `0, 0, 35` |
| 1 | Blue | `240, 100, 35` |
| 2 | Red | `0, 100, 35` |
| 3 | Purple | `285, 100, 35` |
| 4 | Green | `120, 100, 35` |

Change the values in `src/profile_rgb.c` to customize the mapping. Hue is
0-359; saturation and brightness are 0-100.

Profiles 1 and 2 use pure blue/red hues with equal saturation and brightness.
This makes the red-channel peak of profile 2 equal to the blue-channel peak of
profile 1. The native ZMK breathe effect runs at speed 1, approximately a
12-second full fade cycle on ZMK v0.3.

The listener also reapplies the active profile color shortly after boot, so the
indicator does not depend solely on pressing a profile-selection key.

## OLED battery status

The built-in ZMK status screen is enabled with percentage display. On each OLED,
the battery widget reports the local controller's battery. A left-side-only OLED
therefore shows the central/left battery. The existing split battery fetch/proxy
options remain enabled so a connected host can receive split battery data.

`CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE=n` keeps the OLED visible during validation.
After confirming operation, changing it to `y` will reduce OLED power use.

## Build and flash

1. Push this repository to GitHub.
2. Open the repository's **Actions** tab and run the ZMK build workflow.
3. Download the firmware artifact.
4. Flash `helix_left-nice_nano_v2-zmk.uf2` to the left controller and
   `helix_right-nice_nano_v2-zmk.uf2` to the right controller.
5. Select Bluetooth profiles 0-4 and verify the corresponding color.

The repository is pinned to ZMK `v0.3` because the original config uses the
nice!nano v2 board name and OLED/LVGL options from that stable release line.

# Third-party notices

This application was implemented independently. It is not an official ZMK,
Prospector, Seeed, or Zephyr product. No Prospector UI code, images, fonts or
hardware drawings are bundled. No ZMK application code is linked into the scanner.

## Reference projects (not firmware dependencies)

- `t-ogura/zmk-config-prospector`, commit
  `391cd16a33ca0e2b62199d298273ed36cc6de2f9`, MIT.
- `t-ogura/prospector-zmk-module`, v2.2.3 commit
  `72a71aeea816eb0f86ffc5b108b267076ce5f0af`, MIT.
  Used to verify byte layout, advertisement transport, keyboard requirements,
  version/profile masking, channel rules, and sender-side left/right placement.
  Its original notices are retained in `licenses/Prospector-*.txt` for provenance.
  The keyboard-side module remains a separate dependency of the user's ZMK build.

## Firmware dependencies

Exact revisions are in `west.yml` and `docs/dependencies-lock.yml`.

| Component | License / role |
|---|---|
| Zephyr RTOS | Primarily Apache-2.0; individual files may carry other permissive SPDX licenses. Kernel, BLE host + open-source controller, USB CDC, ST7789, SPI, LVGL integration |
| LVGL | MIT, graphics/widgets |
| Montserrat font | SIL OFL 1.1; built-in LVGL 14/24px font |
| Font Awesome 5 glyphs in LVGL built-in font | SIL OFL 1.1 font terms; font file includes symbols even though this UI uses ASCII |
| CMSIS / CMSIS 6 | Apache-2.0, ARM device/core definitions; build dependency |
| Nordic HAL / nrfx | nrfx/MDK files used here are BSD-3-Clause; Nordic integration files also include Nordic's 5-clause license. Target remains a Nordic nRF52840 IC |
| SEGGER module | Fetched through the pinned manifest; RTT is disabled in these configurations and no SEGGER code is intended to be linked |
| GCC libgcc | GPL-3.0 with GCC Runtime Library Exception 3.1; compiler support library. The runtime exception permits eligible independent firmware distributions |

The application LCD overlay adapts ST7789 register initialization values from
Zephyr's `st7789v_waveshare_240x240.overlay`, retaining its authors and Apache-2.0
SPDX header. It uses a new pin assignment, bus configuration and panel definition.

`licenses/` includes the source projects' license texts, font notices, and
Nordic source notices. `licenses/compiled-source-notices.txt` records leading
license/copyright blocks from compilation units used by this prototype, including
notices that differ from their project's top-level license. Keep the notices
alongside distributed binaries. The test UF2 images do not include a bootloader
or Nordic SoftDevice binary; do not redistribute such binaries based on these
application notices alone.

These notices describe this prototype and do not grant rights to a product name,
logo, third-party hardware design, or omitted optional assets. A new board,
additional font, controller backend, or bootloader changes the distribution's
dependency/license set and should be checked at that time.

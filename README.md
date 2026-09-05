# Castaway Lookout

Install Johnny Castaway as a Windows application and screensaver, or run it on
a dedicated Waveshare ESP32 display.

## Choose your platform

| Platform | Download | Installation guide |
| --- | --- | --- |
| Windows 11 x64 | [Windows installer — stable 2026.1.0](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0/JohnnyCastaway-Windows-11-x64-Setup.exe) | [Windows setup](docs/SETUP_GUIDE.md) |
| Waveshare ESP32-S3-Touch-LCD-7, 800×480, N16R8 | [ESP32 flashing ZIP — stable 2026.1.0](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0/JohnnyCastaway-ESP32-S3-Touch-LCD-7-2026.1.0.zip) | [ESP32 setup](docs/FLASH_ESP32_7_TOUCH.md) |

The existing installer and application still use the name **Johnny Castaway**.

[All releases](https://github.com/DrWize/Castaway-Lookout/releases) ·
[2026.1.0 release notes](docs/releases/2026.1.0.md) ·
[Documentation](docs/README.md)

## Features at a glance

| Feature | Windows | ESP32 display |
| --- | --- | --- |
| Johnny Castaway playback | Desktop application or screensaver | Standalone display with a 63-scene catalog |
| Controls | F1 settings and keyboard shortcuts | Authenticated LAN webpage and optional Reviewer sidebar |
| Sound | Optional original sounds | Not supported |
| Clock and weather | Not included | Optional sidebar with local time and Open-Meteo weather |
| Settings | Shared INI for application and screensaver | Saved on the device and retained across reboot |

## Install on Windows

1. Download and run the **Windows 2026.1.0 installer** above.
2. Choose automatic verified data setup or select your existing `scrantic-run.zip`.
3. Choose optional sound and whether to make Johnny your screensaver.
4. Launch **Johnny Castaway** from the Start menu. Press **F1** for settings.

Setup installs for your account; no administrator rights are needed. The
installer is unsigned, so Windows may show an Unknown publisher or SmartScreen
prompt. See [Windows setup and troubleshooting](docs/SETUP_GUIDE.md).

## Install on an ESP32 display

You need the exact **Waveshare ESP32-S3-Touch-LCD-7, 800×480, 16 MB flash /
8 MB PSRAM** board, a Windows 10/11 **x64** computer with internet access, a USB
data cable, and your original `RESOURCE.MAP` and `RESOURCE.001` files.

Download the **ESP32 2026.1.0 ZIP** above, then:

1. Extract the complete ESP32 2026.1.0 ZIP and copy your two resource files into `data`.
2. Connect the board's **UART1 USB-to-UART** port, with the UART switch at UART1.
3. Double-click **FLASH_ESP32.bat**, check the detected hardware and confirm.
4. Wait for completion and about **90 seconds** of startup checks, then follow
   the display's Wi-Fi setup instructions.

No Git, Python installation or ESP-IDF is needed. The package supports only the
specified board; it does not support 7B or 8 MB flash variants. Read the
[complete ESP32 setup and recovery guide](docs/FLASH_ESP32_7_TOUCH.md) before
flashing. Clock/weather, sidebar controls, persistence and smooth playback have been
physically accepted.

## Help

Use the [Windows troubleshooting guide](docs/SETUP_GUIDE.md#troubleshooting) or
[ESP32 troubleshooting guide](docs/FLASH_ESP32_7_TOUCH.md#troubleshooting).
If you still need help, [report an issue](https://github.com/DrWize/Castaway-Lookout/issues)
with your platform, release version and error text. Omit passwords.

Original Sierra/Dynamix game data is not bundled with these downloads and is
not covered by the source license. Derived from
[deckarep's Johnny Castaway port](https://github.com/deckarep/Johnny-Castaway-2026-Public)
and [jc_reborn](https://github.com/jno6809/jc_reborn).
[GPL-3.0-or-later license](LICENSE) · [Attribution](NOTICE.md).

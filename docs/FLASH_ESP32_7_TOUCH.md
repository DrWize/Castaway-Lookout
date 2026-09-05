# Install Castaway Lookout on the ESP32 display

This guide also ships as **FLASHING.md** inside the ESP32 ZIP. All online links
work from either copy. No Git, Python installation, ESP-IDF or developer tools
are needed.

## Check your board first

Use only the **Waveshare ESP32-S3-Touch-LCD-7**, **800×480**, with **16 MB flash
and 8 MB PSRAM (N16R8)**. Compare the board label, module and connectors with the
[official Waveshare board reference](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7).
The 7B, 8 MB flash and other display variants are not supported by this package.

The flasher checks ESP32-S3 chip and memory specifications. It cannot identify
the attached display model. Confirm the exact board yourself before continuing.
Flashing replaces the program currently installed on that board.

## Download and prepare

You need:

- A **Windows 10 or Windows 11 x64** computer with internet access. This ZIP
  does not include a Mac or Linux flasher.
- A USB **data** cable; a charge-only cable will not work.
- Your original **RESOURCE.MAP** and **RESOURCE.001** game files. They are not
  included or downloaded by this ESP32 package. Only the supported matching pair
  is accepted; the helper checks both automatically.
- A **2.4 GHz Wi-Fi network** for the local browser controls.

Download the [**ESP32 2026.1.0 flashing ZIP**](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0/JohnnyCastaway-ESP32-S3-Touch-LCD-7-2026.1.0.zip):
`JohnnyCastaway-ESP32-S3-Touch-LCD-7-2026.1.0.zip`.
The [stable release page](https://github.com/DrWize/Castaway-Lookout/releases/tag/v2026.1.0)
also provides its SHA-256 checksum. GitHub's **Source code** archives are not the
flashing package.

1. Extract the **complete ZIP** to a folder you can write to, such as Documents.
   Do not run it inside File Explorer's ZIP preview.
2. Open the extracted **data** folder and copy `RESOURCE.MAP` and `RESOURCE.001`
   there. Keep their exact names.

```text
Extracted folder/
  FLASH_ESP32.bat          <- double-click this
  Flash-JohnnyEsp32.ps1
  FLASHING.md
  firmware/               <- keep all supplied files together
  data/
    PUT_RESOURCE_FILES_HERE.txt
    RESOURCE.MAP          <- your file
    RESOURCE.001          <- your file
```

## Connect and install

1. Disconnect other ESP32 boards. Connect this board through its **UART1
   USB-to-UART Type-C port** and set the board's UART selector to **UART1**.
   The [Waveshare connector reference](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/FAQ)
   shows how UART1 and UART2 differ.
2. Double-click **FLASH_ESP32.bat**.
3. On first use, the helper downloads and SHA-256-checks Espressif's official
   flashing tool. It checks the firmware and your game files, creates your
   private `data/jcdata.bin`, and searches the COM ports.
4. Check the displayed **ESP32-S3**, **16 MB flash**, **8 MB embedded PSRAM**
   and selected COM port. Press **Enter** to install or **Ctrl+C** to cancel.
5. Wait for **Flash complete**. Do not disconnect the cable while writing.
   The tool verifies the writes and restarts the board automatically.
6. Allow about **90 seconds** for startup checks before normal playback and
   networking begin.

## First start and Wi-Fi

On a new board or one without saved Wi-Fi settings:

1. Read the **Johnny-XXXX** Wi-Fi name and numeric password shown on the display.
2. Connect your phone or computer to that network. A "no internet" message is
   expected; stay connected while completing setup.
3. Open **http://192.168.4.1** in your browser.
4. Enter your **2.4 GHz** Wi-Fi name and password, then create an administrator
   password for Johnny's control page.
5. Reconnect your phone or computer to your home network. Open the displayed
   **http://johnny-xxxx.local** address. If it does not resolve, use the board's
   displayed IP address or find its address in your router's connected devices.
6. Log in using the administrator password you just created. Bookmark the page
   to change playback, picture and sidebar settings later.

After setup, the sidebar defaults to **Off**, so the unused strip is black.
The optional **Clock & weather** sidebar needs a selected city and internet
access. Keep browser controls on your trusted home network.

## Update an existing installation

Use the same ZIP procedure, including the two original resource files. The
flasher does not erase the settings partition: existing Johnny Wi-Fi,
admin password, sidebar and playback settings are retained for this partition
layout. An already configured board normally rejoins its saved network.

There is no over-the-air updater in 2026.1.0. Do not follow developer erase-flash
commands for a normal installation or update. Keep your private game files and
`jcdata.bin` out of shared copies of the package.

## Troubleshooting

| Problem | What to do |
| --- | --- |
| No COM port or board found | Check the data cable, UART1 connector and UART switch; close serial monitors. Check **Device Manager → Ports (COM & LPT)**. If the CH343 device has no driver, use the official driver guidance below. |
| A port appears, but connection fails | Follow the BOOT/RESET steps below and retry. |
| Missing or unsupported game files | Put the supported original pair directly in `data`, with exact filenames. A mixed or modified pair is rejected. |
| Download or checksum error | Check internet access and retry. For a firmware checksum error, download and extract a fresh release ZIP. |
| Flash was interrupted | Reconnect, enter download mode below and rerun the same helper. |
| No picture immediately after flashing | Allow about 90 seconds. Release BOOT and tap RESET if the board remained in download mode. Recheck the exact supported board model. |
| The control page does not open | Rejoin the same home network, use the board's IP address, and check your router's client list. |

For a missing driver, use
[Waveshare's Windows CH343 driver](https://files.waveshare.com/upload/f/f1/CH343SER.7z).
Extract the driver archive and follow its installer instructions, then reconnect
the board. Windows may require administrator approval to install a driver.

To enter download mode:

1. Hold **BOOT**.
2. Tap and release **RESET**, still holding BOOT.
3. Release **BOOT**, then run **FLASH_ESP32.bat** again.

2026.1.0 is the stable ESP32 release. Clock/weather presentation, control
switching, persistence and smoothness have been physically accepted. ESP32
audio and SD-card resource loading are not included.

For help, [report an issue](https://github.com/DrWize/Castaway-Lookout/issues)
with **ESP32 2026.1.0**, your exact board model and the error text; omit passwords.
[Developer documentation](https://github.com/DrWize/Castaway-Lookout/blob/main/esp32/README.md)
is optional and is not needed for these installation steps.

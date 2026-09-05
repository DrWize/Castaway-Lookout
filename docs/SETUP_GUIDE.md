# Install Castaway Lookout on Windows

[Choose a platform](../README.md) · [ESP32 setup](FLASH_ESP32_7_TOUCH.md)

For **Windows 11 x64**. The current Windows release is **2026.1.0 RC3**.
The installer, Start menu entry and program still use **Johnny Castaway**.

## Install

1. [Download the Windows RC3 installer](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0-rc.3/JohnnyCastaway-Windows-11-x64-Setup.exe)
   (`JohnnyCastaway-Windows-11-x64-Setup.exe`).
2. Run it using your normal Windows account. No administrator rights are needed.
3. Choose **Download the verified archive from Internet Archive**, or **Use an
   existing scrantic-run.zip** if you already have that archive.
4. Choose whether to install optional sound effects and set Johnny as your
   current screensaver.
5. Finish setup and open **Johnny Castaway** from the Start menu.

The unsigned installer may trigger an Unknown publisher or SmartScreen prompt.
Use the download linked above; if your computer's policy blocks it, ask its
administrator. Do not disable Windows security settings.

Game data is not bundled with the installer. Automatic setup requires internet
access and verifies the external archive before use. Optional sounds download
separately. The original Sierra/Dynamix content is not covered by this project's
source license.

## Configure the screensaver

If you selected the screensaver option during installation, it is already
registered for your account. Open **Windows Screen Saver Settings** from the
**Johnny Castaway 2026** Start menu folder to choose the idle wait and preview it.

Open the normal **Johnny Castaway** application and press **F1** to adjust
picture, playback and sound settings. The application and screensaver share
those settings. Press **Esc twice within 1.5 seconds** to close the application.
Use **Data and Sound Setup** in the same Start menu folder to retry data setup
or add sounds later.

## Updates and removal

Close Johnny before running a newer installer. Installer upgrades preserve the
shared `JohnnyCastaway.ini` beside the program; the project rename does not
change your installation folder or settings.

Use **Uninstall Johnny Castaway** from the Start menu to remove the application.
Uninstall removes its shared settings file. The separately managed game-data
folder is retained.

## Troubleshooting

| Problem | What to do |
| --- | --- |
| Data download or verification fails | Run **Data and Sound Setup** again. If the external download is unavailable, select your existing supported `scrantic-run.zip`. |
| You have loose `RESOURCE.MAP` and `RESOURCE.001` files | Use the optional portable setup below; the installer's existing-data choice expects the archive. |
| The application cannot find data | Press **F10**, browse to the folder containing the two original resource files, and let Johnny verify them. |
| No sound | Check sound settings with **F1**; run **Data and Sound Setup** if optional sounds were not installed. |
| The screensaver does not start automatically | Open **Windows Screen Saver Settings** from the Start menu, select Johnny, set the wait time and apply. |
| Settings do not persist | Keep `JohnnyCastaway.ini` beside the application and screensaver in a folder your account can write to. |

If setup still fails, [report the exact error](https://github.com/DrWize/Castaway-Lookout/issues)
and mention **Windows RC3**. Clean-machine installer/screensaver acceptance is
still pending before a stable release.

## Optional portable setup

Use this only if you prefer managing files yourself. For automatic screensaver
registration and data setup, use the installer above.

1. Download the [RC3 application](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0-rc.3/JohnnyCastaway-Windows-x64.exe)
   and, optionally, the [RC3 screensaver](https://github.com/DrWize/Castaway-Lookout/releases/download/v2026.1.0-rc.3/JohnnyCastaway-Windows-x64.scr).
2. Put them in a folder you own, such as **Documents\JohnnyCastaway**.
   Rename them to `JohnnyCastaway.exe` and `JohnnyCastaway.scr` respectively.
   Enable file-name extensions in File Explorer if needed.
3. Create a `scrantic` subfolder and copy your original `RESOURCE.MAP` and
   `RESOURCE.001` into it. Optional `sound*.wav` files go in the same subfolder.
4. Open `JohnnyCastaway.exe`. Both programs use the same data folder and shared
   settings. The two original resource files must match the supported hashes;
   the application checks them automatically.

```text
JohnnyCastaway/
  JohnnyCastaway.exe
  JohnnyCastaway.scr       (optional)
  scrantic/
    RESOURCE.MAP
    RESOURCE.001
    sound1.wav            (optional)
```

For manual screensaver command modes and other advanced options, see the
[technical reference](DEVELOPMENT.md#screensaver-modes).

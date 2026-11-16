# Installation

## What you need

A supported, legally acquired Ocarina of Time 3D (USA, Rev 1) game-card dump in `.cci` or `.3ds` format. The filename can be anything. Keep exactly one game file beside the application; multiple candidates or unsupported files block startup.

The single-screen interface and randomizer are included. Optional custom texture packs are separate.

## Windows

1. Extract `SariaUI-1.0.0-windows-x64.zip` into a writable location and open the `SariaUI` folder.
2. Keep `SariaUI.exe`, `oot3d-seed.exe`, its DLLs, `qt.conf`, and the `plugins` folder together.
3. Put your game beside `SariaUI.exe`, then open it.

Windows 10/11 x64 and a Vulkan-capable graphics driver are required. You do not need to install Python. The application is unsigned.

## Linux

1. Extract `SariaUI-1.0.0-linux-x86_64.tar.gz` and open the `SariaUI` folder. If downloading a build artifact from Actions, extract its outer ZIP first.
2. Put the extracted folder somewhere writable under your home directory. Keep the complete release together.
3. Put your game beside `SariaUI.AppImage` and make the AppImage executable if needed.
4. Launch `SariaUI.AppImage`.

A Vulkan-capable graphics driver is required. The randomizer helper and application dependencies are bundled; Python is not required to play.

If FUSE is unavailable, launch from a terminal with:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./SariaUI.AppImage
```

## macOS (Apple Silicon)

1. Extract `SariaUI-1.0.0-macos-arm64.zip`. If downloading a build artifact from Actions, extract its outer ZIP first.
2. Move the entire `SariaUI` folder with Finder to a writable location, such as Documents.
3. Put your game beside `SariaUI.app`, not inside it, then open the app.
4. If macOS blocks the app because the developer cannot be verified, open **System Settings → Privacy & Security → Open Anyway**, then confirm **Open**.

An Apple Silicon Mac (M1 or newer) running macOS 13.4 or newer is required. This package does not support Intel Macs. The randomizer helper and app dependencies are bundled; Homebrew, Python, and developer tools are not required. The app is ad-hoc signed and is not Apple-notarized.

If the app cannot find its adjacent files after downloading, quit it, move the entire extracted folder with Finder, and reopen it. See the [README troubleshooting instructions](README.md#on-mac-it-doesnt-detect-my-3ds-or-cci-file-even-though-it-is-beside-the-app) if the problem persists.

## First launch and later launches

SariaUI opens the game automatically and creates a `user` folder beside the application for saves, settings, custom textures, and randomizer data. On Mac, this folder sits beside `SariaUI.app`, not inside it. There is no separate mod installer or file-selection form.

The game stays in place; SariaUI does not move or duplicate it. Keep the game in the application folder. The app closes silently if no game file is found or if multiple game files are present.

## Settings

Press **F1** (or **Fn–F1** on a Mac using media keys) for General, Graphics, Camera, HUD, Difficulty, QOL, Audio, Controls, and Randomizer. Game touchscreen input is disabled. Controllers are configured automatically; controller and keyboard mappings can be changed under **Controls → Configure controller / keyboard**.

Fullscreen defaults to **F11** on Windows/Linux and **Control–Command–F** on macOS. Change the shortcut under **Controls → Fullscreen shortcut**. **Esc** leaves fullscreen.

The full map is available from the inventory Map tab. The minimap also has a controller shortcut to hide or show it.

For an optional texture pack, use **Graphics → Open texture folder**, install a compatible pack, enable **Use custom textures**, and restart SariaUI after adding or replacing texture files. You can assign a shortcut under Controls to toggle an installed pack on or off.

## Updates and saves

1. Close SariaUI and back up the entire `user` folder.
2. Extract the complete new release into a separate writable folder.
3. Copy your game and the entire `user` folder beside the new application.
4. Launch the new version.

Keep the entire `user` folder, including randomizer data, together when updating or moving to another computer. Preserve your backup if an update fails. Do not delete saves or the `user` folder to troubleshoot.

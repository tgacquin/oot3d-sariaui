# SariaUI

This is a mod to make Ocarina of Time 3D fully functional on one screen. In fact, the bottom screen isn't even accessible.

Play with a built-in randomizer and multiplayer, controller rumble, cleaner and more responsive menus, and smoother dialogue and textboxes.

Every element that could only be interacted with through the bottom screen has now been brought to the top. Previously large HUD elements have also been adapted in an effort to bring it in line with the original game. The goal was to make this game feel like any other console Zelda while also allowing for a very customizable experience so it could feel fresh for returning players.

A custom fork of Azahar is used as the backing engine.

## Features

- Built-in randomizer with presets and support for Original and Master Quest dungeon layouts
- Multiplayer for randomized files, with Host and Join options, visible players, shared progression, and an activity feed
- Controller rumble for impacts, fishing, and nearby secrets
- Cleaner, more responsive menus, with file select, options, saving, and death menus on one screen
- Smoother dialogue camera transitions, textbox animations, text reveal, and animated dialogue indicators
- Frame interpolation up to 120fps with CPU (default) and GPU options, a frame-rate slider, and optional display refresh-rate matching. Defaults to 60fps; select Original in Graphics for 30fps
- Gear, Items, and Map accessible all through one screen with item models and descriptions preserved
- Ocarina song menu accessible on one screen
- On-screen minimap with a controller shortcut to hide or show it
- Redesigned HUD elements (smaller and wider textboxes, thicker magic bar) meant to emulate a console-like experience
- HUD scaling, positioning, spacing, and visibility controls, including grouped button scaling and adjustable heart spacing
- HD text and optional HD numbers
- Difficulty options (damage multiplier, adjustable red potion, blue potion, and bottled fairy healing, no heart, rupee, fairy, or magic drops, disabled ammo pickups, rupee loss on death, and 2×/3× purchase prices)
- Widescreen, increased FOV, and sliders to adjust camera distance
- Right-stick camera controls with adjustable sensitivity, acceleration, deceleration, and axis inversion
- Optional smooth camera pullback
- References in text to the bottom screen and other 3DS-specific features have been replaced with the new controls for a more console-like experience
- Higher-resolution pause-menu backgrounds and adapted world and dungeon maps
- Quality-of-life options, including faster area loading, disabling Navi's unsolicited reminders, and muting the low-health beep
- Custom texture support, with configurable shortcuts for toggling textures and fullscreen
- French and Spanish support with translations for custom button prompts and new dialogue
- Many other minor changes in camera and HUD elements

## Getting started

1. Download the latest release corresponding to your platform.
2. Extract the complete release into a writable folder.
3. Put your legally acquired Ocarina of Time 3D (USA, Rev 1) `.cci` or `.3ds` file in the same folder as the application. Keep only one game file there. On Mac, put it beside `SariaUI.app`, not inside it.
4. Run `SariaUI.exe` on Windows, `SariaUI.app` on Mac, or `SariaUI.AppImage` on Linux. It opens the game automatically and creates a `user` folder beside the application on first launch.

**Press F1 to open settings and adjust graphics, camera, HUD layout, difficulty, quality-of-life options, audio, controls, and randomizer settings.**

Controllers are configured automatically. To adjust bindings, open **F1 → Controls → Controller / keyboard**.

## Updating

1. Close SariaUI and back up the entire `user` folder beside the application.
2. Extract the complete new release into a separate writable folder.
3. Copy your existing `user` folder and game file beside the new application.
4. Launch the new version.

Saves, HUD settings, graphics preferences, custom textures, and randomizer data are kept together in `user`. On Mac, this folder sits beside `SariaUI.app`, not inside it. Keep the entire folder when updating or moving to another computer so your saves and randomizer data stay together. Do not delete your `user` folder to troubleshoot.

## Troubleshooting

### The app closes immediately without opening the game

Make sure exactly one supported game file is beside the application. SariaUI closes silently if no game file is found or if multiple game files are present.

### On Mac, it doesn't detect my .3ds or .cci file even though it is beside the app

Try this:

1. Close SariaUI and open Terminal.
2. Type `xattr -dr com.apple.quarantine `, including the trailing space.
3. Drag `SariaUI.app` into Terminal, then press Return.
4. Reopen SariaUI.

Dragging the app into Terminal supplies its full path. This can resolve macOS quarantine moving the application away from the game file when it launches.

### I get an error saying the main partition is encrypted despite having the right version of the game

Use a decrypted `.cci` or `.3ds` dump of your legally owned Ocarina of Time 3D (USA, Rev 1) game. An encrypted dump cannot be opened without the appropriate keys; renaming its extension will not decrypt it.

## Reporting issues

Found a bug or crash? Please report it in this repository's **Issues** tab.

Include your version, operating system, and steps to reproduce the issue. For randomizer issues, include your seed and settings, and mention whether you were playing alone or in multiplayer.

## Note about this project

AI was used extensively for the purpose of identifying each element the game renders and allowing repositioning, scaling, and transforming of them from the bottom screen to the top screen. This project otherwise would not have been feasible for me to do as Ocarina of Time 3D is not yet decompiled and changes have to be done at the assembly level. However, all repositioning/scaling of the elements for a clean interface was done by me and required a lot of forethought in terms of how the layouts should look. For those unwilling to try this project because of this, [Ship of Harkinian](https://www.shipofharkinian.com/) is also an excellent way to enjoy Ocarina of Time.

This project is currently in beta and likely has many bugs I've yet to see, so make sure you save frequently!

Thank you to the [OoT3D Randomizer](https://github.com/gamestabled/OoT3D_Randomizer) team for their work on the randomizer, and to the Azahar and Citra contributors for the underlying engine!

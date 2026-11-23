# SariaUI

This is a mod to make Ocarina of Time 3D fully functional on one screen. In fact, the bottom screen isn't even accessible.

Every element that could only be interacted with through the bottom screen has now been brought to the top. Previously large HUD elements have also been adapted in an effort to bring it in line with the original game. The goal was to make this game feel like any other console Zelda while also allowing for a very customizable experience so it could feel fresh for returning players. A randomizer has also been integrated.

A custom version of Azahar is used as the backing engine.

## Features

- Single screen HUD designed to emulate a console-like experience
- Widescreen, increased FOV, and up to 120FPS interpolation
- Gear, Items, and Map accessible all through one screen with item models and descriptions preserved
- Built-in randomizer with presets and support for Original and Master Quest dungeon layouts. [Randomizer guide](RANDOMIZER.md)
- Multiplayer for randomized files, with Host and Join options, shared progression, and an activity feed
- Single screen ocarina song list
- Restored rumble functionality from the original N64 version
- Right-stick camera controls with adjustable sensitivity, acceleration, deceleration, and axis inversion
- Difficulty options (damage multiplier, adjustable red potion, blue potion, and bottled fairy healing, no heart, rupee, fairy, or magic drops, disabled ammo pickups, rupee loss on death, and 2×/3× purchase prices)
- HD text and optional HD numbers
- Cleaner, more responsive menus that respond at user's frame rate
- Smoother dialogue camera transitions, textbox animations, text reveal, and animated dialogue indicators
- References in text to the bottom screen and other 3DS-specific features have been replaced with the new controls for a more console-like experience
- Quality-of-life options, including faster area loading and ladder climbing
- Bloom and anti-aliasing
- HUD scaling, positioning, spacing, and visibility controls, including grouped button scaling and adjustable heart spacing
- Custom texture support, with configurable shortcuts for toggling textures and fullscreen
- French and Spanish support with translations for custom button prompts and new dialogue
- Many other minor changes not included here

## Screenshots

<img width="1728" height="971" alt="Screenshot 2026-11-22 at 6 42 16 PM" src="https://github.com/user-attachments/assets/f5ce2a8e-444a-439c-8469-ad698f654976" />

<img width="1728" height="970" alt="Screenshot 2026-11-22 at 6 30 50 PM" src="https://github.com/user-attachments/assets/c6c5a28f-ea08-4887-a702-41ffde18d132" />

<img width="1727" height="972" alt="Screenshot 2026-11-22 at 6 32 13 PM" src="https://github.com/user-attachments/assets/70555802-1734-4955-9d3b-75aabb32dd51" />

<img width="1728" height="971" alt="Screenshot 2026-11-22 at 6 33 22 PM" src="https://github.com/user-attachments/assets/725f546b-e61d-45fc-864b-85b8150f2f4b" />

<img width="1727" height="973" alt="Screenshot 2026-11-22 at 6 32 37 PM" src="https://github.com/user-attachments/assets/1b1cf881-8504-4a03-8901-1d03074bf5db" />

<img width="1728" height="972" alt="Screenshot 2026-11-22 at 6 42 50 PM" src="https://github.com/user-attachments/assets/0fdc9a94-1805-4c3e-8f88-e0d042256c0c" />

<img width="1728" height="974" alt="Screenshot 2026-11-22 at 6 44 17 PM" src="https://github.com/user-attachments/assets/0a15b630-313a-422d-b2f3-f8281bf58f38" />

<img width="1728" height="974" alt="Screenshot 2026-11-22 at 6 43 31 PM" src="https://github.com/user-attachments/assets/b2d7c6a7-342a-4b0b-b3fa-c7406126b653" />

<img width="1728" height="972" alt="Screenshot 2026-11-22 at 6 45 12 PM" src="https://github.com/user-attachments/assets/0c5f6d3a-486d-4968-94f2-9b2218e2cea1" />

<img width="1728" height="972" alt="Screenshot 2026-11-22 at 7 01 41 PM" src="https://github.com/user-attachments/assets/3894d09b-174f-4633-9a93-b1d186682c4a" />



## Getting started

1. Download the latest release corresponding to your platform.
2. Extract the complete release into a writable folder.
3. Put your legally acquired Ocarina of Time 3D (USA, Rev 1) `.cci` or `.3ds` file in the same folder as the application.
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

Include your version, operating system, and steps to reproduce the issue. For randomizer issues, mention whether you were playing alone or in multiplayer and attach the matching file from `user/config/oot3d-randomizer/` beside SariaUI.

- If it failed at the Randomizing prompt, attach `last-generation.json` before trying another seed.
- If it happened during gameplay, attach `last-played.json` before opening another randomized file. Include the area and what you were doing.

Both reports include the seed, quest mode, difficulty and randomizer settings. To reproduce generation with the matching build, run `oot3d-seed --replay last-generation.json replayed-seed.json`. The same command accepts `last-played.json`.

## Note about this project

AI was used extensively in the making of this project, although layouts were manually made by me after being able to scale and reposition individual elements.

Thank you to the [OoT3D Randomizer](https://github.com/gamestabled/OoT3D_Randomizer) team for their work on the randomizer, and to the Azahar and Citra contributors for the underlying engine!

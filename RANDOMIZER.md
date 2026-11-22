# Playing the randomizer

The built-in randomizer is based on **OoT3D Randomizer**. Thank you to gymnast86, gamestabled, HylianFreddy, cjohnson57, Roberto-Nessy, Kewlan, and everyone who contributed to the project. SariaUI integrates their generator and runtime into the game’s file-select and multiplayer experience. [Original project](https://github.com/gamestabled/OoT3D_Randomizer) · [Full contributor list](https://github.com/gamestabled/OoT3D_Randomizer/graphs/contributors)

## Start your first run

1. At file select, press **F1 → Randomizer** and turn on **Enable randomizer for new files**.
2. Under **General → Presets**, select **Beginner** and press **Apply preset**. You start with the Kokiri Sword, an ocarina, and dungeon maps. Keys stay in their dungeons, and Gossip Stones give stronger hints.
3. Leave **Seed** blank for a fresh shuffle. Close settings and select an empty file slot.
4. On the quest screen, check that the randomizer is on and choose **Original** for the original dungeon layouts or **Master Quest** for the alternate layouts. Wait for **Randomizing** to finish, then name and start your file.

## Choose what gets shuffled

A seed determines where items land under the rules you choose. To recreate a run, use the same SariaUI version, seed text, quest layout, randomizer rules, and difficulty settings. During a run, open **F1 → Randomizer → General → Current file** to see the seed and **View saved rules**. Editing settings prepares the next file you create.

| Preset | What to expect |
| --- | --- |
| **Defaults** | The standard OoT3D Randomizer rules. |
| **Beginner** | Glitchless logic, stronger hints, six dungeon rewards required, and no Ganon’s Castle trials. |
| **Standard** | Familiar entrances, shuffled songs and Gold Skulltula tokens, and key rings that can turn up outside their dungeons. |
| **Advanced** | Random starting age, shuffled dungeon and boss entrances, and boss keys that can turn up anywhere. |

To customize a preset, apply it first. **Shuffles** controls which items can move, **Starting Items** controls what you begin with, and **Check Locations** lets you exclude places from progression. Use **F1 → Difficulty** before creating the file to set damage, healing, and shop price multipliers.

**Why do I already have a Spiritual Stone?** The **Link’s Pocket** setting gives you a starting reward. With **Dungeon Reward** selected, a stone or medallion goes straight into your inventory. Seeing the Kokiri’s Emerald before entering the Deku Tree is a valid outcome.

## Play with friends

Use the same SariaUI version. **Multiplayer** and **Shared Progress** are enabled by default for new randomized files. The host creates the seed once. Guests receive the host’s file when they join.

| | Host | Guest |
| --- | --- | --- |
| First session | Select your randomized file. On the screen with **Start**, choose **Host** and wait for your guest. | Select an empty slot. On the quest screen, enable the randomizer and choose **Join**. Enter your player name when prompted. |
| Enter the game | Once your guest connects, choose **Start**. | At **Waiting for host to start**, wait for the host to begin. Your file name is your multiplayer name. |
| Next session | Open the same file and choose **Host** again. | Open the file you received and choose **Join**. SariaUI looks for its matching room ID. |

A guest joining an established run receives the host’s progress, including inventory, dungeon rewards, Gold Skulltula count, and opened chests. With **Shared Progress** on, the group can split up. If one player opens a chest, the other should find it opened. If one collects a Gold Skulltula token, the shared count should increase. **Shared Health**, **Shared Rupees**, and **Shared Ammo** control whether those resources continue to sync during play. Each player can choose their own tunic color and whether to display names or the activity log.

**If the guest keeps searching**, open **F1 → Randomizer → Multiplayer → Advanced connection settings**. For local play, enable **Search local network** and use the same discovery port on both devices (default `24873`). For Tailscale, enter the host’s Tailscale IPv4 address on the guest device and choose **Apply connection settings**. If you connect but game synchronization fails, choose **Retry synchronization** in the Multiplayer settings.

## Keep your run when updating or moving devices

Close SariaUI and copy the entire `user` folder so the save slots and their matching randomizer data travel together. On Windows it sits beside `SariaUI.exe`, on macOS beside `SariaUI.app`, and on Linux or Steam Deck beside `SariaUI.AppImage`. Place that folder beside the application on the new installation. [Update instructions](README.md#updating)

## Report a failed seed or a crash

Open `user/config/oot3d-randomizer/` beside SariaUI and copy the report that matches what happened.

| When it happened | File to attach | When to copy it |
| --- | --- | --- |
| During **Randomizing** | `last-generation.json` | Before trying another seed |
| While playing a randomized file | `last-played.json` | Before opening another randomized file |

Both reports contain the seed, quest layout, difficulty, and randomizer settings. Include your SariaUI version, platform, and what you were doing. For multiplayer, say whether you were the host or a guest and what the other player saw. [Report a SariaUI issue](https://github.com/tgacquin/oot3d-sariaui/issues)

## Credits and licenses

OoT3D Randomizer builds on the original **OoT Randomizer**. Its license credits **Amazing Ampharos** and **Junglechief87**, and acknowledges **LLCoolDave** and **KevinCathcart** for their work on the Zelda ALttP Entrance Randomizer. SariaUI’s Beginner, Standard, and Advanced presets were adapted from Ship of Harkinian’s presets. [Upstream license and acknowledgments](externals/oot3d-randomizer/LICENSE.md) · [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) · [SariaUI credits](CREDITS.md)

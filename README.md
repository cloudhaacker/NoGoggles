# NoGoggles

**Play SteamVR games on your monitor, without a headset.**

NoGoggles is a pretend VR headset for SteamVR. Games think a headset is plugged in, but the picture shows up in a normal window on your screen: one clean view, no split-screen, no lens warping. You turn your "head" with a gamepad stick, the mouse or the arrow keys.

It was made for **Psychonauts in the Rhombus of Ruin**, a VR-only game you play seated with a gamepad by looking at things. It should also work with other SteamVR games that use a gamepad and let your head do the aiming.

The game files aren't touched at all. NoGoggles is a SteamVR driver, so it only plugs into SteamVR.

## Who made this

**John (cloudhaacker)** came up with the idea, designed how it works and does all the testing.
**Claude (Anthropic's AI)** wrote the code.

## Download and install

1. Install **SteamVR** from Steam (free). Start it once, let it complain that no headset was found, then close it.
2. Download the latest zip from [Releases](../../releases), unzip it and double-click **Install NoGoggles.bat**.
   Windows may show a blue "Windows protected your PC" box because the program isn't signed. Click **More info**, then **Run anyway**.
3. Launch your game from Steam. A NoGoggles window shows the game.

Uninstall from **Windows Settings > Apps > Installed apps > NoGoggles**.

## Controls

| Key / button | What it does |
|---|---|
| Right stick | Turn your head |
| Click right stick (R3) | Look straight ahead again |
| Click left stick (L3) | Hide/show the center dot |
| Hold Back + Start | Close SteamVR when you're done |
| Arrow keys | Turn your head |
| F8 | Mouse look on/off |
| F9 | Look straight ahead again |
| F7 | Hide/show the center dot |
| Hold F10 | Close SteamVR when you're done |

The game's own buttons work as normal on your gamepad. Everything can be changed in **NoGoggles Settings** (there's a shortcut on your desktop), including per-game settings.

## What works with it

- **Works well:** games played with a gamepad where you aim or pick things by looking, like Psychonauts in the Rhombus of Ruin.
- **Won't work:** games that need VR hand controllers (Half-Life: Alyx, Beat Saber and so on), and games sold only on the Meta/Oculus store, since those don't use SteamVR.

Tested with: Psychonauts in the Rhombus of Ruin (Steam). Tried another game? Open an issue and say how it went.

## Known tips

- If a game says **"HMD Tracking Error"**, adjust `HeadHeight` in the settings. 0.7 is right for Rhombus of Ruin.
- Games only read the gamepad while their own window is active. NoGoggles handles this: it keeps its view on top and makes the game's window active behind it. If the gamepad stops responding, Alt+Tab to the game's window.
- If SteamVR suggests switching to "Direct Display Mode", ignore it.

## How it works (for the curious)

- It's an OpenVR driver (`driver_nogoggles.dll`) that adds a headset to SteamVR.
- The headset uses SteamVR's "extended mode": its screen is a desktop window. Both eyes draw to the same full-size area with eye spacing set to 0, so you get a single clean image.
- It reports the headset as "being worn" and gives SteamVR a ready-made room setup, so games don't think it's idle or uncalibrated.
- Head direction is sent about 250 times a second, along with turning speed so SteamVR's frame prediction doesn't leave black edges while you turn.
- The center dot and on-screen messages are small click-through windows on top.
- It's built without the Visual C++ runtime, so it only needs Windows itself.

## Building from source

The driver is cross-compiled from Linux with clang and lld. No Visual Studio or Windows SDK is needed.

```sh
./build.sh      # builds build/driver_nogoggles.dll
./package.sh    # builds and makes out/NoGoggles-v<version>.zip
```

Needs `clang`, `lld` (`lld-link`) and `llvm-dlltool`. The OpenVR driver header is from [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr) (BSD license, see `src/OPENVR_LICENSE.txt`).

## License

MIT. See [LICENSE](LICENSE).

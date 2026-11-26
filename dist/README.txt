NoGoggles - play SteamVR games on your monitor, no headset
============================================================
Made for Psychonauts in the Rhombus of Ruin (Steam version). Version 0.6.

NoGoggles is a pretend headset for SteamVR. The game thinks a VR headset is
plugged in, but the picture shows up in a normal window on your screen, with
no split-screen and no lens warping. You turn your "head" with the mouse,
the arrow keys or a gamepad stick.

The game files aren't touched at all. NoGoggles only plugs into SteamVR.

Who made this
-------------
John came up with the idea, designed how it should work and tests it.
Claude (Anthropic's AI) wrote the code.


WHAT YOU NEED
-------------
- Windows 10 or 11
- Steam, with SteamVR installed (free - search "SteamVR" in the Steam Store)
- Psychonauts in the Rhombus of Ruin on Steam
- A gamepad (the game needs one; an Xbox-style controller is easiest)


INSTALL
-------
1. Start SteamVR once from Steam. It'll say no headset was found. That's
   fine, just close it. (It only needs to do this once, to set itself up.)
2. Double-click "Install NoGoggles.bat".
   If Windows shows a blue "Windows protected your PC" box, click
   "More info" and then "Run anyway". It shows up because this is a
   homemade program without a paid signature.
3. That's it. The installer copies NoGoggles to your user folder, so you
   can delete this download folder afterwards if you like.


PLAYING
-------
1. Launch Psychonauts in the Rhombus of Ruin from Steam.
2. SteamVR starts, and a "NoGoggles" window shows the game.
3. That's it. NoGoggles makes the game's window the active one (so the
   gamepad works) while keeping the NoGoggles window in front where you
   can see it. If the gamepad ever stops responding, press Alt+Tab to the
   game's window - the NoGoggles view stays on top.

There will be three windows. That's normal:
  - SteamVR Status  (the little status box - leave it alone, and don't
                     switch to "Direct Display Mode" if it suggests it)
  - NoGoggles       (the window to play in)
  - The game's own window (a narrow, squashed preview hidden behind
                     NoGoggles - NoGoggles keeps it active for you)

Controls (all of them can be changed):
  Right stick        turn your head
  Arrow keys         turn your head
  F8                 mouse look on/off (holds the pointer in place while on)
  F9                 snap back to looking straight ahead
  F7                 hide/show the center dot
  Hold F10 (1 sec)   close SteamVR after you quit the game

A short message appears at the top of the screen when you use these keys,
so you know it worked.

The white dot in the middle of the screen is where you're looking. The game
picks whatever is under it. You can resize it or turn it off in the settings.

The game's own buttons work as normal on the gamepad.


SETTINGS
--------
Double-click "NoGoggles Settings" on your desktop. Every setting has a
note explaining it. Things worth trying:
  - Fullscreen = yes          to fill your main monitor
  - FieldOfView = 80          if things look stretched at the edges
  - GamepadHoldButton = LB    if the right stick also does something in the game
  - Smoothing = 0.5           if head movement feels jerky
  - HeadHeight                0.7 is right for this game; if you see
                              "HMD Tracking Error", this is the setting
Looking settings apply within a second. Display settings apply the next
time SteamVR starts.

Per-game settings: at the bottom of the settings file you can add a
[Game:Name] section with the game's program name. While that game is
running, its settings replace the normal ones, and a message shows
which game's settings are in use. The settings file explains how.


IF SOMETHING GOES WRONG
-----------------------
SteamVR keeps a log at:
   C:\Program Files (x86)\Steam\logs\vrserver.txt
Search it for "NoGoggles". The lines there show what the driver saw.

UNINSTALL
---------
Windows Settings > Apps > Installed apps > NoGoggles > Uninstall.
(Or double-click "Uninstall NoGoggles.bat" if you still have the download.)
It removes NoGoggles and puts SteamVR's settings back the way they were.


TECH NOTES
----------
- It's a SteamVR (OpenVR) driver: nogoggles\bin\win64\driver_nogoggles.dll.
- It works in SteamVR's "extended mode". The view goes to a desktop window,
  and both eyes draw to the same full-size spot with eye spacing set to 0,
  so you get one clean picture.
- Head direction is sent to SteamVR about 250 times a second.
- It's built without the Visual C++ runtime, so it only needs Windows itself.
- Source code is in the "source" folder.

# One Piece: Pirate Warriors 4 Fix
[![Patreon-Button](https://github.com/user-attachments/assets/9cb844b7-5c8a-41d6-8b3d-80a349e2d261)](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9) <br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/OPPW4Fix/total.svg)](https://github.com/Lyall/OPPW4Fix/releases)

This is a fix for One Piece: Pirate Warriors 4 that adds ultrawide/narrower support, unlocked framerate and much more.

## Features

### General
- Custom resolution support.
- Intro logo skip.
- Adjust gameplay FOV.
- Adjust framerate cap. (Experimental)
- Adjust shadow resolution.
- Increase (native) render texture resolution.

### Ultrawide/narrower
- Support for any aspect ratio.
- Fixed HUD stretching.
- Fixed movie stretching.

## Installation
- Grab the latest release of OPPW4Fix from [here.](https://github.com/Lyall/OPPW4Fix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. ("**steamapps\common\OPPW4**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="version=n,b" %command%` to the launch options.

## Configuration
- See **OPPW4Fix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

- There are several HUD issues at <16:9.

## Screenshots
| ![ezgif-6-a0826394f9](https://github.com/user-attachments/assets/fcee6463-9927-47c8-a3c2-9287de1564d7) |
|:--:|
| Gameplay |

## Credits
Thanks to Terry on the WSGF Discord for providing a copy of the game! <br/>
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.

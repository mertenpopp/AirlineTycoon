# Building and running the code

Before building, remember to clone the submodules:
```
git submodule update --init
```

If you want to run the game, you'll also need original assets to run the game.
You can purchase them on [GoG](https://www.gog.com/game/airline_tycoon_deluxe).

Pick your environment:
- Windows on Visual Studio 2022
- Windows on Visual Studio Code
- Linux
- MacOS on Visual Studio Code

## Windows with Visual Studio 2022

### Installing SDL2 dependencies
SDL2 version 2.10 is the minimum required version

The `dl-sdl2.ps1` powershell script will automatically download and install SDL2 in this directory - ready to be used by the AT project.

You can run it with this command: `powershell -ExecutionPolicy Bypass -File ./dl-sdl2.ps1 `

---

If you want to download it yourself, you can use these commands:
```ps
wget "https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.20.1/SDL2_ttf-devel-2.20.1-VC.zip" -outfile "sdl2-ttf.zip"
wget "https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.6.2/SDL2_mixer-devel-2.6.2-VC.zip" -outfile "sdl2-mixer.zip"
wget "https://github.com/libsdl-org/SDL_image/releases/download/release-2.6.2/SDL2_image-devel-2.6.2-VC.zip" -outfile "sdl2-image.zip"
wget "https://github.com/libsdl-org/SDL/releases/download/release-2.24.2/SDL2-devel-2.24.2-VC.zip" -outfile "sdl2.zip"

Expand-Archive -Path sdl2.zip -DestinationPath .\temp\
Move-Item -Path .\temp\SDL2* -Destination .\sdl2\
Expand-Archive -Path sdl2-ttf.zip -DestinationPath .\temp\
Move-Item -Path .\temp\SDL2* -Destination .\sdl2-ttf\
Expand-Archive -Path sdl2-mixer.zip -DestinationPath .\temp\
Move-Item -Path .\temp\SDL2* -Destination .\sdl2-mixer\
Expand-Archive -Path sdl2-image.zip -DestinationPath .\temp\
Move-Item -Path .\temp\SDL2* -Destination .\sdl2-image\
rm .\temp\
rm .\sdl2.zip
rm .\sdl2-mixer.zip
rm .\sdl2-ttf.zip
rm .\sdl2-image.zip
```

### Building
To build the application, open the project solution with Visual Studio, select your configuration and then build like any other VS project.

## Windows on Visual Studio Code

> You don't have to manually handle SDL - it will be installed if required by CMake. Just follow the steps

- Download [Visual Studio Build Tools](https://visualstudio.microsoft.com/fr/downloads/#build-tools-for-visual-studio-2022)
    - Execute, choose the "Desktop C++ Development" and install
- (Optional) Copy the original assets (from GoG) in the `gamefiles` folder. You can let the .md file in there.
- Open or install [Visual Studio Code](https://code.visualstudio.com/) and open the source code
- Install the 'C/C++ Extension Pack'. This should be proposed by a popup at startup.
    - You can always do install it later by opening the extension panel (`CTRL + SHIFT + X`) and install it from there
- Once installed, you should be asked to configure the project, click `Yes`
    - You can always do this later by opening the command panel (`CTRL + P`) then type and select `CMake: Select a kit`
- Select any `amd64` from the `Visual Studio Build Tools`. Configuration should begin in the `Output` window. Wait until it's done.
- Once Configuration is done, you can run and debug the project with `CTRL + F5`
    - Game will build but won't start unless you added your assets into the `gamefiles` folder

## Linux x86
(AMD64 only with MultiArch for i686)

SDL2 should be included in most packaging tools (apt, etc.) under the name `libsdl2*-dev` or `SDL2*-devel`.
CMake will automatically detect those installations for you in the make step.

Installing should be as simple as
- Install `libjansson-dev libsdl2*-dev`
- Run `cmake -B ./build -S .` - wait for the configuration to finish
- Run `cmake --build ./build --config Release`

### Fedora AMD64
sudo dnf install jansson-devel.i686 SDL2*-devel.i686

### Ubuntu x64
sudo apt-get install libjansson-dev:i386 libsdl2*-dev:i386

### Building on AMD64
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=./Toolchain-i686.cmake ..

## MacOS on Visual Studio Code

> Make sure you cloned **git submodules**

- Install [Homebrew](https://brew.sh) if you don't have it already
- Install `cmake` and `SDL2` with Homebrew using `brew install cmake sdl2 sdl2_mixer sdl2_image sdl2_ttf jansson`
- Open or install [Visual Studio Code](https://code.visualstudio.com/) and open the source code
- Install the 'C/C++ Extension Pack'. This should be proposed by a popup at startup.
    - You can always do install it later by opening the extension panel (`CTRL + SHIFT + X`) and install it from there
- Once installed, you should be asked to configure the project, click `Yes`
    - You can always do this later by opening the command panel (`CTRL + P`) then type and select `CMake: Select a kit`
- Select `arm64` if you have an ARM Apple Chip; or 'amd64' for any other processor, from the `Visual Studio Build Tools`. Configuration should begin in the `Output` window. Wait until it's done.
- Once Configuration is done, you can run and debug the project with `CTRL + F5`
    - Game will build but won't start unless you added your assets into the `gamefiles` folder

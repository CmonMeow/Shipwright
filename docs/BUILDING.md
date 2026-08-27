# Building Ship of Harkinian

## Windows

Requires:
  * At least 8GB of RAM (machines with 4GB have seen compiler failures)
  * Visual Studio 2022 Community Edition with the C++ feature set
  * One of the Windows SDKs that comes with Visual Studio, for example the current Windows 10 version 10.0.19041.0
  * The `MSVC v143 - VS 2022 C++ build tools` component of Visual Studio
  * Python 3 (can be installed manually or as part of Visual Studio)
  * Git (can be installed manually or as part of Visual Studio)
  * Cmake (can be installed via chocolatey or manually)

During installation, check the "Desktop development with C++" feature set:

![image](https://user-images.githubusercontent.com/30329717/183511274-d11aceea-7900-46ec-acb6-3f2cc110021a.png)
Doing so should also check one of the Windows SDKs by default.  Then, in the installation details in the right-hand column, make sure you also check the v143 toolset. This is often done by default.

It is recommended that you install Python and Git standalone, the install process in VS Installer has given some issues in the past.

1. Clone the Ship of Harkinian repository

_Note: Be sure to either clone with the ``--recursive`` flag or do ``git submodule update --init`` after cloning to pull in the libultraship submodule!_

2. Use OTRExporter to produce an oot.o2r from a compatible ROM before running the game.

_Note: Instructions assume using powershell_
```powershell
# Navigate to the Shipwright repo within powershell. ie: cd "C:\yourpath\Shipwright"
cd Shipwright

# Setup cmake project
# Add `-DCMAKE_BUILD_TYPE:STRING=Release` if you're packaging
# Add `-DSUPPRESS_WARNINGS=0` to prevent suppression of warnings from LUS and decomp (src) files. set to 1 to re-enable suppression
& 'C:\Program Files\CMake\bin\cmake' -S . -B "build/x64" -G "Visual Studio 17 2022" -T v143 -A x64

# Compile project
# Add `--config Release` if you're packaging
& 'C:\Program Files\CMake\bin\cmake.exe' --build .\build\x64

# Now you can run the executable in .\build\x64 or run in Visual Studio
```

### Developing SoH
With the cmake build system you have two options for working on the project:

#### Visual Studio
To develop using Visual Studio you only need to use cmake to generate the solution file:
```powershell
# Generates Ship.slnx at build/x64 for Visual Studio
& 'C:\Program Files\CMake\bin\cmake' -S . -B "build/x64" -G "Visual Studio 17 2022" -T v143 -A x64
```

From a Developer PowerShell for VS 2022, you can also run the repository script:
```powershell
.\scripts\windows\configure-msvc.ps1
```
Open build/x64/Ship.slnx in Visual Studio after the script finishes. The script
uses the Visual Studio generator and the MSVC v143 toolset directly.

#### Visual Studio Code or another editor
To develop using Visual Studio Code or another editor you only need to open the repository in it.
To build you'll need to follow the instructions from the building section.

_Note: If you're using Visual Studio Code, the [CMake Tools plugin](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) makes it very easy to just press run and debug._

_Experimental: You can also use another build system entirely rather than MSVC like [Ninja](https://ninja-build.org/) for possibly better performance._


### Generating the distributable
After compiling the project you can generate the distributable by running:
```powershell
# Go to build folder
cd "build/x64"
# Generate
& 'C:\Program Files\CMake\bin\cpack.exe' -G ZIP
```

### Additional CMake Targets
#### Clean
```powershell
# If you need to clean the project you can run
C:\Program Files\CMake\bin\cmake.exe --build build-cmake --target clean
```

#### Regenerate Asset Headers
```powershell
# If you need to regenerate the asset headers to check them into source
C:\Program Files\CMake\bin\cmake.exe --build build-cmake --target ExtractAssetHeaders
```

## Getting CI to work on your fork

The CI works via [Github Actions](https://github.com/features/actions) where we mostly make use of machines hosted by Github; except for the very first step of the CI process called "Extract assets". This steps extracts assets from the game file and generates an "assets" folder in `soh/`.

To get this step working on your fork, you'll need to add a machine to your own repository as a self-hosted runner via "Settings > Actions > Runners" in your repository settings. Make sure to add the 'asset-builder' tag to your newly added runner to assign it to run this step. To setup your runner as a service read the docs [here](https://docs.github.com/en/actions/hosting-your-own-runners/configuring-the-self-hosted-runner-application-as-a-service?platform=linux).

### Runner on Windows
You'll have to enable the ability to run unsigned scripts through PowerShell. To do this, open Powershell as administrator and run `set-executionpolicy remotesigned`. Most dependencies get installed as part of the CI process. You will also need to separately install 7z and add it to the PATH so `7z` can be run as a command. [Chocolatey](https://chocolatey.org/) or other package managers can be used to install it easily.

### Runner on UNIX systems
If you're on macOS or Linux take a look at `macports-deps.txt` or `apt-deps.txt` to see the dependencies expected to be on your machine.

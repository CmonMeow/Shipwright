# Source layout

- `Ship.slnx` is the canonical Visual Studio solution.
- `msvc/` contains the permanent MSVC project files.
- `shipwright/` contains the game and Path Engine integration.
- `libultraship/` contains the engine code that will be pruned and merged into the game source over time.

Open `Ship.slnx` in Visual Studio, select `Release` and `x64`, then build the solution. No project-generation step is required.

Executables and symbols are written to `../bin`. Intermediate files and internal static libraries are written to `../obj`.

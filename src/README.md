# Source layout

- `Ship.slnx` is the canonical Visual Studio solution.
- `shipwright/` contains the game and Path Engine integration.
- `libultraship/` contains the engine code that will be pruned and merged into the game source over time.

The solution references Visual Studio project files generated in `../build/x64`. If that disposable directory is absent, configure it once from a Visual Studio Developer PowerShell opened at the repository root:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 18 2026" -A x64
```

Release executables, symbols, libraries, configuration, and the merged `oot.o2r` are written to `../bin`.

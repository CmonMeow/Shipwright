# Ocarina

A Windows/OpenGL PC game based on the Ocarina of Time engine, customized for mouse-and-keyboard combat, fishing, and server-authoritative multiplayer.

## Build

Open [`src/Game.slnx`](src/Game.slnx) in Visual Studio and build `Release | x64`.

Outputs are written to `bin/`:

- `GameClient.exe` — game client
- `GameServer.exe` — dedicated server
- `VoiceClient.exe` — standalone voice and text client

The source tree is split into `src/game` for game code and `src/engine` for the retained PC runtime and renderer.

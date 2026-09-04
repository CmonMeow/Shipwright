# Rendering subsystem

Implementation of a Fast3D renderer for games built originally for the Nintendo 64 platform.

The active renderer is OpenGL. Native Win32/WGL process and window ownership lives
directly in `src/game/platform/win32/WinMain.cpp`.

# Ownership

`WinMain` owns `OpenGLPresentation`, which borrows the HWND and HDC
created alongside it and handles only buffer presentation, frame pacing, title
telemetry, and client dimensions. `Engine::Rendering::GameRenderer` borrows that
presentation object and owns the Fast3D interpreter and OpenGL rendering API.

Neither rendering component registers a window class, creates or destroys a native
window or WGL context, processes messages, or owns application lifetime.

# Frame flow

`WinMain` pumps Win32 messages and calls `Application::RunFrame()` once per native
game frame. That application boundary advances the retained graph frame; the
retained game submits its display list through `NativeFramePresenter`,
which asks `GameRenderer` to interpret and present the required interpolation
samples.

Retained C display-list handlers reach the active interpreter through the narrow
ABI bridge required by that code. They do not own renderer or application state.


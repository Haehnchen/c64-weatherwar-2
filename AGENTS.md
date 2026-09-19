# Agent notes

- C++20/SDL3 port of Weatherwar II. Entry point: `src/main.cpp`.
- `src/basic`: BASIC semantics; `game`: match flow; `scenes`: screens; `weather`: attacks; `video`: rendering; `audio`: SID playback.
- Keep game rules independent of SDL. Preserve graphics, input and sound behavior.
- Tests mirror the source folders. Run `make doctor` and `make check` after code changes.
- Never open windows on the real desktop during automated checks. Use dummy SDL drivers or isolated Xvfb; unset `WAYLAND_DISPLAY` and set `SDL_VIDEODRIVER=x11`, `GDK_BACKEND=x11` for Xvfb. `make run` is manual only.
- Keep comments short and in English.

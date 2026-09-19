#!/usr/bin/env python3
"""Exercise SDL opening/name/weapon routes on isolated Xvfb and compare pixels."""
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


class WindowAttributes(C.Structure):
    _fields_ = [(name, C.c_int) for name in ("x", "y", "width", "height", "border_width", "depth")] + [
        ("visual", C.c_void_p), ("root", C.c_ulong), ("window_class", C.c_int),
        ("bit_gravity", C.c_int), ("win_gravity", C.c_int), ("backing_store", C.c_int),
        ("backing_planes", C.c_ulong), ("backing_pixel", C.c_ulong), ("save_under", C.c_int),
        ("colormap", C.c_ulong), ("map_installed", C.c_int), ("map_state", C.c_int),
        ("all_event_masks", C.c_long), ("your_event_mask", C.c_long),
        ("do_not_propagate_mask", C.c_long), ("override_redirect", C.c_int), ("screen", C.c_void_p)]


class XErrorEvent(C.Structure):
    _fields_ = [("type", C.c_int), ("display", C.c_void_p), ("resourceid", C.c_ulong),
                ("serial", C.c_ulong), ("error_code", C.c_ubyte),
                ("request_code", C.c_ubyte), ("minor_code", C.c_ubyte)]


def main():
    # xvfb-run supplies a private Xauthority cookie. Refuse an ordinary desktop
    # invocation, and force X11 so SDL cannot escape to an inherited Wayland session.
    authority = os.environ.get("XAUTHORITY", "")
    if "xvfb-run." not in authority:
        raise RuntimeError("Run only under xvfb-run -a; desktop execution is forbidden")
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    xtst = C.CDLL(ctypes.util.find_library("Xtst"))
    x11.XOpenDisplay.argtypes = [C.c_char_p]; x11.XOpenDisplay.restype = C.c_void_p
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]; x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
    x11.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
    x11.XFree.argtypes = [C.c_void_p]
    x11.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
    x11.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)]
    x11.XResizeWindow.argtypes = [C.c_void_p, C.c_ulong, C.c_uint, C.c_uint]
    x11.XDefaultScreen.argtypes = [C.c_void_p]; x11.XDefaultScreen.restype = C.c_int
    x11.XDisplayWidth.argtypes = [C.c_void_p, C.c_int]; x11.XDisplayWidth.restype = C.c_int
    x11.XDisplayHeight.argtypes = [C.c_void_p, C.c_int]; x11.XDisplayHeight.restype = C.c_int
    x11.XStringToKeysym.argtypes = [C.c_char_p]; x11.XStringToKeysym.restype = C.c_ulong
    x11.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]; x11.XKeysymToKeycode.restype = C.c_uint
    x11.XFlush.argtypes = [C.c_void_p]
    x11.XCloseDisplay.argtypes = [C.c_void_p]
    x11.XSync.argtypes = [C.c_void_p, C.c_int]
    x11.XSetErrorHandler.argtypes = [C.c_void_p]
    x11.XSetErrorHandler.restype = C.c_void_p
    xtst.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    display = x11.XOpenDisplay(None)
    if not display: raise RuntimeError("Run with xvfb-run -a")
    setup_path = sys.argv[1] if len(sys.argv) == 2 else None
    if setup_path not in (None, "--window-controls", "--setup-early", "--setup-n", "--setup-instructions", "--setup-l", "--setup-t", "--setup-computer", "--setup-statistics", "--setup-quit", "--setup-nature", "--setup-match", "--setup-match-tie", "--setup-match-left", "--setup-numeric"):
        raise ValueError("Unknown setup UI path")
    build = Path(os.environ.get("BUILD_DIR", ROOT / "build/debug")).resolve()
    output = build / "ui-checks" / (setup_path or "instructions").removeprefix("--")
    output.mkdir(parents=True, exist_ok=True)
    executable = build / "weatherwar"
    exporter = build / "tests/native_dump"
    subprocess.run([str(exporter), "--dump-opening", str(output)], check=True)
    if setup_path:
        subprocess.run([str(exporter), "--dump-setup", str(output)], check=True)
        subprocess.run([str(exporter), "--dump-turn", str(output)], check=True)
        subprocess.run([str(exporter), "--dump-attack", str(output / "attacks")], check=True)
        if setup_path == "--setup-computer":
            subprocess.run([str(exporter), "--dump-computer", str(output / "computer")], check=True)
        if setup_path == "--setup-nature":
            subprocess.run([str(exporter), "--dump-nature", str(output / "nature")], check=True)
        if setup_path in ("--setup-match", "--setup-match-tie", "--setup-match-left"):
            subprocess.run([str(exporter), setup_path.replace("--setup-", "--dump-"), str(output / "match")], check=True)
        if setup_path == "--setup-numeric":
            subprocess.run([str(exporter), "--dump-numeric", str(output / "numeric")], check=True)
    with (output / "native.log").open("w") as log:
        isolated_env = {key: value for key, value in os.environ.items() if key != "WAYLAND_DISPLAY"}
        isolated_env.update(SDL_VIDEODRIVER="x11", SDL_AUDIODRIVER="dummy", GDK_BACKEND="x11")
        process = subprocess.Popen([str(executable)], env=isolated_env, stdout=log, stderr=subprocess.STDOUT)
        try:
            window = None
            discovery_errors = []

            @C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XErrorEvent))
            def discovery_error(_display, error):
                # SDL may replace its initial X window while creating the
                # renderer. Only tolerate stale-window errors during discovery.
                if error.contents.error_code != 3:
                    discovery_errors.append(error.contents.error_code)
                return 0

            old_handler = x11.XSetErrorHandler(discovery_error)
            candidate_window, candidate_since = None, 0.0
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and window is None:
                found = None
                root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                x11.XQueryTree(display, x11.XDefaultRootWindow(display), C.byref(root), C.byref(parent), C.byref(children), C.byref(count))
                for i in range(count.value):
                    name = C.c_void_p()
                    if x11.XFetchName(display, children[i], C.byref(name)) and name.value:
                        if C.string_at(name) == b"Weatherwar II - F11: Fullscreen":
                            attributes = WindowAttributes()
                            if x11.XGetWindowAttributes(display, children[i], C.byref(attributes)) and attributes.map_state == 2:
                                found = children[i]
                        x11.XFree(name)
                if children: x11.XFree(children)
                x11.XSync(display, 0)
                if discovery_errors: raise RuntimeError(f"X11 discovery errors: {discovery_errors}")
                if process.poll() is not None: raise RuntimeError("Native UI exited unexpectedly")
                if found != candidate_window:
                    candidate_window, candidate_since = found, time.monotonic()
                elif found is not None and time.monotonic() - candidate_since >= 0.3:
                    window = found
                if window is None: time.sleep(0.05)
            x11.XSetErrorHandler(old_handler)
            if window is None: raise RuntimeError("Native window not found")
            attributes = WindowAttributes()
            while time.monotonic() < deadline:
                if x11.XGetWindowAttributes(display, window, C.byref(attributes)) and attributes.map_state == 2:
                    break
                time.sleep(0.05)
            else: raise RuntimeError("Native window was not mapped on the private display")
            x11.XSetInputFocus(display, window, 1, 0)
            x11.XFlush(display)
            # The real dummy audio device consumes the startup at wall-clock speed.
            if setup_path != "--setup-early": time.sleep(4.5)

            def refresh_window():
                nonlocal window
                # Fullscreen may reparent SDL's window under an unnamed wrapper.
                def find_named(parent_window):
                    root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                    x11.XQueryTree(display, parent_window, C.byref(root), C.byref(parent), C.byref(children), C.byref(count))
                    child_ids = [children[i] for i in range(count.value)]
                    if children: x11.XFree(children)
                    for child in child_ids:
                        name = C.c_void_p()
                        matches = False
                        if x11.XFetchName(display, child, C.byref(name)) and name.value:
                            matches = C.string_at(name) == b"Weatherwar II - F11: Fullscreen"
                            x11.XFree(name)
                        if matches: return child
                        nested = find_named(child)
                        if nested is not None: return nested
                    return None

                found = find_named(x11.XDefaultRootWindow(display))
                if found is None: raise RuntimeError("SDL window missing after fullscreen transition")
                window = found
                x11.XSetInputFocus(display, window, 1, 0)
                x11.XFlush(display)

            def key(name):
                code = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode()))
                if not code: raise RuntimeError(f"No X keycode for {name}")
                xtst.XTestFakeKeyEvent(display, code, 1, 0)
                xtst.XTestFakeKeyEvent(display, code, 0, 0)
                x11.XFlush(display)
                time.sleep(0.15)
                if name == "F11":
                    time.sleep(0.2)
                    refresh_window()

            def compare(page, filename):
                refresh_window()
                name = C.c_void_p()
                if not x11.XFetchName(display, window, C.byref(name)) or not name.value:
                    raise RuntimeError("Window title missing")
                title = C.string_at(name.value).decode()
                x11.XFree(name)
                if title != "Weatherwar II - F11: Fullscreen": raise RuntimeError(f"Unexpected window title: {title}")
                x11.XGetWindowAttributes(display, window, C.byref(attributes))
                width, height = attributes.width, attributes.height
                game_width = min(width, height * 4 / 3)
                game_height = game_width * 3 / 4
                left = (width - game_width) / 2
                game_top = (height - game_height) / 2
                candidates = [output / page / "frame.ppm"]
                cursor = output / (page + "-cursor") / "frame.ppm"
                if cursor.is_file(): candidates.append(cursor)
                allowed = []
                for candidate in candidates:
                    pixels = candidate.read_bytes().split(b"\n", 3)[3]
                    scaled = bytearray()
                    for row in range(height):
                        source_row = min(199, max(0, int((row + 0.5 - game_top) * 200 / game_height)))
                        source = pixels[source_row * 960:(source_row + 1) * 960]
                        for column in range(width):
                            if not (left <= column + 0.5 < left + game_width and game_top <= row + 0.5 < game_top + game_height):
                                scaled.extend(b"\0\0\0")
                            else:
                                x = min(319, max(0, int((column + 0.5 - left) * 320 / game_width))) * 3
                                scaled.extend(source[x:x + 3])
                    allowed.append(scaled)
                actual = bytearray(subprocess.run(["import", "-window", str(window), "-depth", "8", "rgb:-"], capture_output=True, check=True).stdout)
                if len(actual) != width * height * 3: raise RuntimeError("Unexpected screenshot dimensions")
                subprocess.run(["import", "-window", str(window), str(output / filename)], check=True)
                if not any(actual == candidate for candidate in allowed):
                    mismatch = [i // 3 for i in range(0, len(actual), 3) if actual[i:i+3] != allowed[0][i:i+3]]
                    raise RuntimeError(f"SDL input/render mismatch: {page}; {len(mismatch)} pixels, first coordinates {[(p % width, p // width) for p in mismatch[:12]]}")

            if setup_path == "--window-controls":
                def size():
                    refresh_window()
                    x11.XGetWindowAttributes(display, window, C.byref(attributes))
                    return attributes.width, attributes.height

                compare("question", "window-initial.png")
                x11.XResizeWindow(display, window, 1104, 600)
                x11.XFlush(display)
                time.sleep(0.5)
                compare("question", "window-resized.png")
                original_size = size()
                key("F11"); time.sleep(0.5)
                screen = x11.XDefaultScreen(display)
                desktop = (x11.XDisplayWidth(display, screen), x11.XDisplayHeight(display, screen))
                if size() != desktop:
                    wm = subprocess.run(["xprop", "-root", "_NET_SUPPORTING_WM_CHECK"],
                                        capture_output=True, text=True, check=True).stdout
                    if "window id #" in wm:
                        raise RuntimeError(f"Window manager did not enter fullscreen: {size()} vs {desktop}")
                    # Bare Xvfb has no EWMH window manager to apply desktop
                    # fullscreen state. Do not pretend to test restoration of
                    # a state this environment did not enter.
                    compare("question", "fullscreen-request.png")
                    print(f"Title, resize and 4:3 pixels passed. Fullscreen/restore requires a window manager; skipped on bare Xvfb ({size()} vs {desktop}).")
                    return
                compare("question", "fullscreen-f11.png")
                key("Escape"); time.sleep(0.5)
                if size() != original_size: raise RuntimeError("Escape did not restore window size")
                key("F11"); key("F11"); time.sleep(0.5)
                if size() != original_size: raise RuntimeError("F11 did not restore window size")
                compare("question", "window-restored.png")
                key("y"); key("Return")
                key("F11"); key("F11"); time.sleep(0.5)
                compare("instructions-one", "instructions-after-fullscreen.png")
                key("Escape"); process.wait(timeout=5)
                print("Standard window title, resize, F11 fullscreen, Escape restore and undistorted 4:3 game pixels passed.")
                return

            if setup_path == "--setup-early":
                # Eight bytes fit the original ten-byte keyboard buffer, before
                # the title melody finishes. ALICE must survive the setup handoff.
                key("n"); key("Return")
                for letter in "alice": key(letter)
                key("Return")
                time.sleep(5)
                for letter in "bob": key(letter)
                key("Return")
                time.sleep(4)
                compare("weapon", "early-weapon.png")
                key("h")
                enter = x11.XKeysymToKeycode(display, x11.XStringToKeysym(b"Return"))
                xtst.XTestFakeKeyEvent(display, enter, 1, 0); x11.XFlush(display)
                time.sleep(1.5)
                compare("charge-H", "held-return-charge.png")
                xtst.XTestFakeKeyEvent(display, enter, 0, 0); x11.XFlush(display)
                zero = x11.XKeysymToKeycode(display, x11.XStringToKeysym(b"0"))
                xtst.XTestFakeKeyEvent(display, zero, 1, 0); x11.XFlush(display)
                time.sleep(1.5)
                xtst.XTestFakeKeyEvent(display, zero, 0, 0); x11.XFlush(display)
                compare("charge-H-zero", "held-digit-charge.png")
                key("1"); key("2")
                delete = x11.XKeysymToKeycode(display, x11.XStringToKeysym(b"BackSpace"))
                xtst.XTestFakeKeyEvent(display, delete, 1, 0); x11.XFlush(display)
                time.sleep(1.5)
                xtst.XTestFakeKeyEvent(display, delete, 0, 0); x11.XFlush(display)
                compare("charge-H", "held-delete-charge.png")
                key("0")
                key("Return")
                duration = json.loads((output / "attacks/H0/timing.json").read_text())["end_cycle"] / 985248
                time.sleep(duration + 2)
                compare(str(output / "attacks/H0/next-weapon"), "early-next-player.png")
                key("Escape")
                if process.wait(timeout=5): raise RuntimeError("Early-input UI returned an error")
                print("SDL buffered title/name handoff, held Return and following attack passed.")
                return
            if setup_path in ("--setup-n", "--setup-l", "--setup-t", "--setup-computer", "--setup-statistics", "--setup-quit", "--setup-nature", "--setup-match", "--setup-match-tie", "--setup-match-left", "--setup-numeric"):
                key("n"); key("Return")
            else:
                key("y"); key("Return")
                compare("instructions-one", "instructions-one.png")
                key("space")
                compare("instructions-two", "instructions-two.png")
                key("r")
                compare("instructions-one", "instructions-repeat.png")
                if setup_path:
                    key("space"); key("space")
            if setup_path == "--setup-computer":
                time.sleep(2)
                for letter in "computer": key(letter)
                key("Return")
                time.sleep(2)
                for letter in "bob": key(letter)
                key("Return")
                duration = json.loads((output / "computer/move-0/timing.json").read_text())["end_cycle"] / 985248
                time.sleep(duration + 5)
                compare(str(output / "computer/move-1/human-prompt"), "computer-first-attack-ui.png")
                key("h"); key("Return"); key("0"); key("Return")
                duration = json.loads((output / "computer/move-1/timing.json").read_text())["end_cycle"] / 985248
                time.sleep(duration + 3)
                duration = json.loads((output / "computer/move-2/timing.json").read_text())["end_cycle"] / 985248
                time.sleep(duration + 3.0)
                compare(str(output / "computer/move-3/human-prompt"), "computer-second-attack-ui.png")
            elif setup_path:
                time.sleep(2)
                key("a"); key("Return") # rejected short name must not play a tone
                for letter in "alice": key(letter)
                key("Return")
                time.sleep(2)
                for letter in "bob": key(letter)
                key("Return")
                time.sleep(4)
                compare("weapon", "weapon-ui.png")
                if setup_path == "--setup-numeric":
                    key("h"); key("Return")
                    compare(str(output / "numeric/overflow/charge-prompt"), "numeric-charge.png")
                    for character in "1e39": key(character)
                    key("Return")
                    compare(str(output / "numeric/overflow/end"), "numeric-overflow.png")
                    key("0"); key("Return")
                    duration = json.loads((output / "numeric/overflow/recovery-timing.json").read_text())["end_cycle"] / 985248
                    time.sleep(duration + 3)
                    compare(str(output / "numeric/overflow/recovered"), "numeric-recovered.png")
                    key("Escape")
                    if process.wait(timeout=5): raise RuntimeError("Native numeric UI returned an error")
                    print("SDL overflow retry and next turn passed; native pixels match.")
                    return
                if setup_path in ("--setup-match", "--setup-match-tie", "--setup-match-left"):
                    match = output / "match"
                    moves = sorted(match.glob("move-*"), key=lambda path: int(path.name.split("-")[1]))
                    for move in moves:
                        if (move / "human-prompt").is_dir():
                            compare(str(move / "human-prompt"), f"match-{move.name}.png")
                            controls = json.loads((move / "input.json").read_text())
                            key(controls["weapon"].lower()); key("Return")
                            for character in controls["charge"]:
                                key("minus" if character == "-" else character)
                            key("Return")
                        elif (move / "nature-entry").is_dir():
                            duration = json.loads((move / "nature-intro/timing.json").read_text())["end_cycle"] / 985248
                            time.sleep(duration)
                        else:
                            break  # The headless sequence has reached the result.
                        duration = json.loads((move / "timing.json").read_text())["end_cycle"] / 985248
                        time.sleep(duration + 3)
                        if process.poll() is not None:
                            raise RuntimeError("Native UI exited during complete match")
                    duration = json.loads((match / "result/timing.json").read_text())["end_cycle"] / 985248
                    time.sleep(duration + 3)
                    compare(str(match / "result-prompt"), "match-result.png")
                    key("s"); key("Return")
                    compare(str(match / "result-statistics"), "match-statistics.png")
                    duration = json.loads((output / "statistics-timing.json").read_text())["delay_cycles"] / 985248
                    time.sleep(duration + 1)
                    compare(str(match / "result-statistics-return"), "match-statistics-return.png")
                    key("y"); key("Return")
                    for scene in ("replay-music", "replay-board"):
                        duration = json.loads((match / scene / "timing.json").read_text())["end_cycle"] / 985248
                        time.sleep(duration)
                    time.sleep(4)
                    compare(str(match / "replay-human-prompt"), "match-replay.png")
                    key("q"); key("Return")
                    compare(str(match / "replay-Q"), "match-replay-quit.png")
                    key("Escape")
                    if process.wait(timeout=5): raise RuntimeError("Native match UI returned an error")
                    print("SDL natural complete match/result/S/Y/replay/Q path passed; native prompt pixels match. Original timing/PCM acceptance separate.")
                    return
                if setup_path == "--setup-nature":
                    nature = output / "nature"
                    # Follow normal inputs only: sixteen HAIL-0 attacks reach
                    # the first automatic nature event without state injection.
                    for move in range(16):
                        compare(str(nature / f"move-{move}/human-prompt"), f"nature-prompt-{move}.png")
                        key("h"); key("Return"); key("0"); key("Return")
                        duration = json.loads((nature / f"move-{move}/timing.json").read_text())["end_cycle"] / 985248
                        time.sleep(duration + 3)
                        if process.poll() is not None:
                            raise RuntimeError("Native UI exited during natural sequence")
                    intro = json.loads((nature / "move-16/nature-intro/timing.json").read_text())["end_cycle"] / 985248
                    weather = json.loads((nature / "move-16/timing.json").read_text())["end_cycle"] / 985248
                    time.sleep(intro + weather + 3)
                    compare(str(nature / "move-17/human-prompt"), "after-nature-ui.png")
                    key("Escape")
                    if process.wait(timeout=5): raise RuntimeError("Native nature UI returned an error")
                    print("SDL natural 16-turn/nature/next-player path passed; prompt pixels match. Not an original PCM comparison.")
                    return
                if setup_path == "--setup-quit":
                    key("q"); key("Return")
                    compare("quit-final", "quit-final-ui.png")
                    key("Escape")
                    if process.wait(timeout=5): raise RuntimeError("Native quit UI returned an error")
                    print("SDL Q/score/END path passed; displayed pixels match.")
                    return
                if setup_path == "--setup-statistics":
                    key("s"); key("Return")
                    compare("statistics-shown", "statistics-shown-ui.png")
                    # Buffered keys during the original FOR/NEXT pause require
                    # a separate paired input capture; this path sends none.
                    duration = json.loads((output / "statistics-timing.json").read_text())["delay_cycles"] / 985248
                    time.sleep(duration + 1)
                    compare("statistics-return", "statistics-return-ui.png")
                    key("s"); key("Return")
                    key("h"); key("Return")
                    time.sleep(duration + 1)
                    compare("statistics-buffered-charge", "statistics-buffered-charge-ui.png")
                    key("Escape")
                    if process.wait(timeout=5): raise RuntimeError("Native statistics UI returned an error")
                    print("SDL statistics display/pause/return passed; displayed pixels match.")
                    return
                key("x"); key("Return")
                compare("invalid-weapon", "invalid-weapon-ui.png")
                weapon, charge, case = {"--setup-n": ("h", "0", "H0"), "--setup-instructions": ("r", "100", "R100"),
                                       "--setup-l": ("l", "0", "L0"), "--setup-t": ("t", "150", "T150")}[setup_path]
                key(weapon); key("Return")
                compare("charge-" + weapon.upper(), "charge-ui.png")
                for digit in charge: key(digit)
                key("Return")
                duration = json.loads((output / "attacks" / case / "timing.json").read_text())["end_cycle"] / 985248
                time.sleep(duration + 2)
                compare(str(output / "attacks" / case / "next-weapon"), "next-player-ui.png")
            key("Escape")
            if process.wait(timeout=5): raise RuntimeError("Native UI returned an error")
            print(f"SDL input/render path {setup_path or 'instructions'} passed; displayed pixels match.")
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()

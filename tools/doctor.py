"""Check native build prerequisites."""
import shutil
import subprocess

from setup_audio import PREFIX, is_installed


def main():
    failures = []
    for command in ("git", "python3", "cmake", "ninja", "c++", "ctest", "pkg-config"):
        found = shutil.which(command)
        print(f"{'OK' if found else 'MISSING'} {command}")
        if not found:
            failures.append(command)
    if shutil.which("pkg-config"):
        result = subprocess.run(["pkg-config", "--modversion", "sdl3"],
                                capture_output=True, text=True)
        print(f"SDL3: {(result.stdout or result.stderr).strip()}")
        if result.returncode:
            failures.append("SDL3 development package")
    ready = is_installed(PREFIX)
    print(f"{'OK' if ready else 'SETUP NEEDED'} pinned SID library")
    if not ready:
        failures.append("SID library (make audio-setup)")
    if failures:
        print("Missing: " + ", ".join(failures))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())

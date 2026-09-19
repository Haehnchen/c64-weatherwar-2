BUILD_DIR ?= build/debug
RELEASE_BUILD_DIR ?= build/release-build
CMAKE ?= cmake
PYTHON ?= python3
UI_CASE ?= --setup-early
.DEFAULT_GOAL := help
.PHONY: help doctor audio-setup configure build run test smoke check check-ui release clean

help:
	@echo 'doctor | audio-setup | build | run | check | check-ui | release | clean'

doctor:
	$(PYTHON) tools/doctor.py

audio-setup:
	$(PYTHON) tools/setup_audio.py

configure: audio-setup
	$(CMAKE) -S . -B "$(BUILD_DIR)" -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel

run: build
	"$(BUILD_DIR)/weatherwar"

test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

smoke: build
	SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy "$(BUILD_DIR)/weatherwar" --smoke-test

check: test

check-ui: build
	env -u WAYLAND_DISPLAY BUILD_DIR="$(BUILD_DIR)" SDL_VIDEODRIVER=x11 GDK_BACKEND=x11 SDL_AUDIODRIVER=dummy xvfb-run -a $(PYTHON) tools/check_opening_ui.py $(UI_CASE)

release: audio-setup
	$(CMAKE) -S . -B "$(RELEASE_BUILD_DIR)" -G Ninja -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DWEATHERWAR_RELEASE=ON
	$(CMAKE) --build "$(RELEASE_BUILD_DIR)" --target release --parallel

clean:
	$(CMAKE) --build "$(BUILD_DIR)" --target clean

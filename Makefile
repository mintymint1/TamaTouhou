# Every build starts here. Details: docs/development.md
#
#   make setup     create the Python venv (.venv) with the theme tools and PlatformIO
#   make run       PC simulator: the real game in a window, with debug keys (needs cmake)
#   make test      engine + UI-logic unit tests (needs only a C++17 compiler)
#   make board     firmware for the ESP32-S3 board
#   make flash     flash firmware + theme art to a board on USB
#   make monitor   the board's serial console
#   make release   board build -> web/firmware/ (the files the web flasher installs)
#   make web       serve web/ on http://localhost:8000 to try the flasher locally
#   make data      regenerate the theme tables + pack the art (the others run it first)
#   make check     what CI runs: theme validation, generated files up to date, tests
#   make clean     remove build outputs
#
# THEME=<id> picks the theme pack (default touhou). ARGS="..." passes options to the simulator.
THEME  ?= touhou
VENV   := .venv
PYTHON := $(VENV)/bin/python
PIO    := $(abspath $(VENV)/bin/pio)
STAMP  := $(VENV)/.installed
ENV    := amoled175

.PHONY: setup run test data board flash monitor release web check clean

setup: $(STAMP)

$(STAMP): tools/requirements.txt
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install --quiet --upgrade pip
	$(VENV)/bin/pip install --quiet -r tools/requirements.txt
	@touch $@

data: $(STAMP)
	$(PYTHON) tools/build_theme.py $(THEME) --pack

run: data
	@$(MAKE) --no-print-directory -C firmware/sim run ARGS="$(ARGS)"

test:
	@$(MAKE) --no-print-directory -C firmware/test/host

board: data
	cd firmware && $(PIO) run -e $(ENV)

flash: data
	cd firmware && $(PIO) run -e $(ENV) -t upload && $(PIO) run -e $(ENV) -t uploadfs

monitor: $(STAMP)
	cd firmware && $(PIO) device monitor

release: board
	cd firmware && $(PIO) run -e $(ENV) -t buildfs
	$(PYTHON) tools/make_release.py --theme $(THEME)

web:
	@echo "Web flasher on http://localhost:8000 (Chrome or Edge; Ctrl+C to stop)"
	cd web && python3 -m http.server 8000

check: $(STAMP)
	$(PYTHON) tools/build_theme.py $(THEME) --check
	$(PYTHON) tools/build_theme.py $(THEME)
	@if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
	  git diff --quiet -- firmware/lib/theme_gen || \
	  { echo "firmware/lib/theme_gen is out of date: run 'make data' and commit it"; exit 1; }; fi
	@$(MAKE) --no-print-directory test

clean:
	@$(MAKE) --no-print-directory -C firmware/sim clean
	rm -rf firmware/.pio/build firmware/test/host/build firmware/data web/firmware

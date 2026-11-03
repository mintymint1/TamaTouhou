# The PC simulator (SDL3 via CMake). Usually run from the repo root: `make run`.
# The first build downloads SDL3 once (pinned, into firmware/.deps) and compiles it, ~1-2 min.
#   make           build the simulator
#   make run       build + start it (pack the art first with `make data` at the repo root)
#   make clean     remove the build (the SDL3 download stays)
CMAKE ?= cmake
JOBS  ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
BUILD := build

.PHONY: all run clean
all: $(BUILD)/CMakeCache.txt
	@$(CMAKE) --build $(BUILD) --parallel $(JOBS)

$(BUILD)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=RelWithDebInfo

run: all
	./$(BUILD)/tamatouhou-sim $(ARGS)

clean:
	rm -rf $(BUILD)

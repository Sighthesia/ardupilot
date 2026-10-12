# AGENTS.md — ArduPilot agent notes

Personal fork for experimenting with ArduPilot (vehicles fly/drive/dive). Verify APIs/params/MAVLink against source, don't invent them.

## Layout

- Vehicles: `ArduCopter/`, `ArduPlane/`, `Rover/`, `ArduSub/`, `Blimp/`, `AntennaTracker/`, `Tools/AP_Periph/`. Each has `mode_*.cpp`, `Parameters.*`, `GCS_*.cpp`, `wscript` (library list).
- `libraries/AP_<Name>/` shared drivers, `AC_<Name>/` Copter/QuadPlane control, `AR_<Name>/` Rover. Per library: main class, `*_Backend.*`, `*_config.h` feature flags, optional `tests/` + `examples/`.
- HAL: `libraries/AP_HAL/` interface only; `AP_HAL_ChibiOS/`, `AP_HAL_Linux/`, `AP_HAL_SITL/`, `AP_HAL_ESP32/` are backends. Shared code must use HAL (`AP_HAL::millis()/micros()`), never platform-specific calls.
- `Tools/autotest/` SITL suites (`arducopter.py`, `arduplane.py`, `rover.py`, `ardusub.py`, ...), `Tools/scripts/` CI helpers (`allowed_subsystems.py`, `check_branch_conventions.py`, `build_options.py`), `Tools/CodeStyle/astylerc`.
- ChibiOS hwdefs: `libraries/AP_HAL_ChibiOS/hwdef/`.

## Build (waf, from repo root, never with sudo)

```sh
./waf configure --board sitl && ./waf copter   # plane|rover|sub|heli|antennatracker|AP_Periph
./waf --targets tests/test_math                # single unit-test binary
./waf list_boards; ./waf list                   # boards / all targets
./waf check --alltests                          # build + run all unit tests
```

Incremental builds are the norm; don't `clean`/`distclean` unless configure is broken. Binary lands in `build/<board>/bin/`.

## Test

```sh
Tools/autotest/autotest.py build.Copter test.Copter.RTLYaw   # one SITL test (build.<Vehicle> + test.<Suite>.<Method>)
Tools/autotest/autotest.py build.Copter test.Copter           # full vehicle suite (slow, ~hours)
./build/sitl/tests/test_math                                  # run a built gtest directly
```

SITL suites are slow; prefer one focused test plus relevant `libraries/<lib>/tests/`. Python autotest files carry `AP_FLAKE8_CLEAN`.

## C++ style (`Tools/CodeStyle/astylerc`: linux braces, 4 spaces, LF, `add-braces`)

- `#pragma once`, not `#ifndef` guards. Format only changed lines, not whole files.
- Float compares: `is_zero()/is_positive()/is_negative()`; user messages: `GCS_SEND_TEXT()`; time: `AP_HAL::millis()/micros()`.
- Feature code in `#if AP_<FEATURE>_ENABLED` / `#endif // AP_<FEATURE>_ENABLED`; non-trivial flash cost also needs an entry in `Tools/scripts/build_options.py`. Core code must compile with optional features off.
- Check touched files: `python3 Tools/scripts/run_astyle.py` (or `astyle --options=Tools/CodeStyle/astylerc --dry-run <files>`).

## Python style

- `flake8` max-line-length 127 (see `.flake8`); `ruff` line-length 127 with `force-single-line` isort; `black` line-length 120 applies **only** to `libraries/AP_DDS/**` and `Tools/ros2/**`. New files add the `AP_FLAKE8_CLEAN` marker.

## Parameters

Document above each `AP_GROUPINFO` with `// @Param:`, `@DisplayName:`, `@Description:`, `@Values:`/`@Bitmask:`/`@Range:`, `@Units:`, `@User: Standard|Advanced`, `@RebootRequired:` where applicable. Full name ≤16 chars. Never change existing `AP_GROUPINFO` index numbers (breaks stored configs).

## Commits (personal fork, relaxed)

- No enforced convention; `Subsystem: short description` (e.g. `AP_GPS: fix ...`) is optional but handy for history. Merge commits and `fixup!` are fine.
- Reference: `Tools/scripts/allowed_subsystems.py` only matters if cherry-picking upstream.

## Pitfalls (break things, not policy)

- Don't bypass `AP_<FEATURE>_ENABLED` guards, put platform code in shared libs, or change existing param indices (breaks stored configs).
- Don't touch `modules/` (upstream submodules).

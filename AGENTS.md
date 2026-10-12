# AGENTS.md — ArduPilot agent notes

Safety-critical autopilot (vehicles fly/drive/dive). Keep changes minimal, build + test, never fabricate results. Verify APIs/params/MAVLink against source, don't invent them. Flag uncertain control-loop / failsafe / EKF logic for human review.

## Layout

- Vehicles: `ArduCopter/`, `ArduPlane/`, `Rover/`, `ArduSub/`, `Blimp/`, `AntennaTracker/`, `Tools/AP_Periph/`. Each has `mode_*.cpp`, `Parameters.*`, `GCS_*.cpp`, `wscript` (library list).
- `libraries/AP_<Name>/` shared drivers, `AC_<Name>/` Copter/QuadPlane control, `AR_<Name>/` Rover. Per library: main class, `*_Backend.*`, `*_config.h` feature flags, optional `tests/` + `examples/`.
- HAL: `libraries/AP_HAL/` interface only; `AP_HAL_ChibiOS/`, `AP_HAL_Linux/`, `AP_HAL_SITL/`, `AP_HAL_ESP32/` are backends. Shared code must use HAL (`AP_HAL::millis()/micros()`), never platform-specific calls.
- `Tools/autotest/` SITL suites (`arducopter.py`, `arduplane.py`, `rover.py`, `ardusub.py`, ...), `Tools/scripts/` CI helpers (`allowed_subsystems.py`, `check_branch_conventions.py`, `build_options.py`), `Tools/CodeStyle/astylerc`.
- ChibiOS hwdefs: `libraries/AP_HAL_ChibiOS/hwdef/`. Never touch `modules/` (upstream submodules) or `build/`.

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

SITL suites are expensive/flaky; prefer one focused test plus relevant `libraries/<lib>/tests/`. Python autotest files carry `AP_FLAKE8_CLEAN`. CI also runs ChibiOS board builds, size tracking, and replay tests — a SITL-only pass is not sufficient for HAL/hwdef changes.

## C++ style (`Tools/CodeStyle/astylerc`: linux braces, 4 spaces, LF, `add-braces`)

- `#pragma once`, not `#ifndef` guards. Format only changed lines, not whole files.
- Float compares: `is_zero()/is_positive()/is_negative()`; user messages: `GCS_SEND_TEXT()`; time: `AP_HAL::millis()/micros()`.
- Feature code in `#if AP_<FEATURE>_ENABLED` / `#endif // AP_<FEATURE>_ENABLED`; non-trivial flash cost also needs an entry in `Tools/scripts/build_options.py`. Core code must compile with optional features off.
- Check touched files: `python3 Tools/scripts/run_astyle.py` (or `astyle --options=Tools/CodeStyle/astylerc --dry-run <files>`).

## Python style

- `flake8` max-line-length 127 (see `.flake8`); `ruff` line-length 127 with `force-single-line` isort; `black` line-length 120 applies **only** to `libraries/AP_DDS/**` and `Tools/ros2/**`. New files add the `AP_FLAKE8_CLEAN` marker.

## Parameters

Document above each `AP_GROUPINFO` with `// @Param:`, `@DisplayName:`, `@Description:`, `@Values:`/`@Bitmask:`/`@Range:`, `@Units:`, `@User: Standard|Advanced`, `@RebootRequired:` where applicable. Full name ≤16 chars. Never change existing `AP_GROUPINFO` index numbers (breaks stored configs).

## Commits (CI-enforced)

- First line `Subsystem: short imperative description` (<~72 chars), e.g. `AP_GPS: fix ...`, `Copter: fix ...`. No merge commits, no `fixup!`; rebase onto master.
- One subsystem per commit: every file must belong to the declared prefix. Source of truth: `Tools/scripts/allowed_subsystems.py` (`allowed_subsystems.subsystems_for_path()`).
- Multi-subsystem work: commit once, then `git subsystems-split` (`Tools/gittools/git-subsystems-split`, `-n` for dry-run); verify with `python3 Tools/scripts/check_branch_conventions.py`.
- This file (`AGENTS.md`) maps to the `Tools:` subsystem.

## Do not

- Bypass `AP_<FEATURE>_ENABLED` guards, add platform code to shared libs, add deps/flash cost lightly, or change param indices.
- Large speculative refactors, gratuitous moves/comments, weakening tests, or duplicating an open (<6 mo) PR.
- Push PRs to upstream master for resume/exercise purposes; disclose AI assistance in real PRs with test evidence (SITL log / unit test run).

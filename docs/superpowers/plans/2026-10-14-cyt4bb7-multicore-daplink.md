# CYT4BB7 Multicore DAPLink Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a truthful, safe preflight and documentation workflow for CYT4BB7 CM7/CM0+ build discovery and OpenOCD/pyOCD + DAPLink flashing preparation without inventing unsupported image formats, flash addresses, or target algorithms.

**Architecture:** Keep ArduPilot's existing Infineon application build as the CM7 path. Add a standalone Python preflight that inspects the checked-out target dependency, detects CM7/CM0+ artifacts, checks local tool availability and optionally probes DAPLink; add a separate flashing wrapper that defaults to dry-run and refuses writes until an explicit target configuration, flash algorithm, image type, and address are supplied. Improve the Waf dependency error and document the verified boundaries.

**Tech Stack:** Python 3.9+, argparse, pathlib, subprocess, JSON/text inspection, Waf Python hooks, arm-none-eabi toolchain, OpenOCD, pyOCD, CMSIS-DAP/DAPLink.

## Global Constraints

- Do not fabricate CYT4BB7 CM7/CM0+ startup addresses, linker addresses, image headers, or flash algorithms.
- Do not claim dual-core build or flashing support unless CM7 and CM0+ inputs and a validated packaging rule are present.
- Flashing is opt-in and must refuse to write by default; `--dry-run` and preflight modes must never reset, erase, or program the target.
- Do not initialize or modify Git submodules automatically from a build/preflight script.
- Preserve the existing AM32 KISS, SCB3, UART, DMA, and AP_Param behavior.
- Use ASCII in new Python code and concise comments only for safety-critical decisions.
- Do not modify the user-owned untracked `docs/ref/` contents.

## File Map

- Create `Tools/scripts/check_cyt4bb7_multicore.py`: read-only dependency, core-artifact, toolchain, and DAPLink preflight with machine-readable status lines.
- Create `Tools/scripts/flash_cyt4bb7_daplink.py`: guarded pyOCD/OpenOCD command wrapper; dry-run by default and no guessed target configuration.
- Modify `Tools/ardupilotwaf/infineon.py:39-113`: replace the current `None.abspath()` traceback with an actionable dependency error before adding include paths or sources.
- Create `docs/CYT4BB7_MULTICORE_BUILD_FLASH.md`: build matrix, CM7/CM0+ discovery rules, DAPLink probe workflow, explicit unsupported states, and operator examples.
- Create `Tools/scripts/tests/test_cyt4bb7_tools.py`: subprocess-free unit tests for discovery, status classification, command construction, and write refusal.

### Task 1: Make the Infineon dependency failure actionable

**Files:**
- Modify: `Tools/ardupilotwaf/infineon.py:39-113`
- Test: `Tools/scripts/tests/test_cyt4bb7_tools.py`

**Interfaces:**
- Consumes: repository source root and the required `modules/infineon/TARGET_CYT4BB7` paths.
- Produces: a deterministic `ConfigurationError`/`WafError` message naming the missing path and the non-mutating recovery command.

- [ ] **Step 1: Add a helper test for missing dependency diagnostics.**

```python
def test_missing_generated_source_message():
    message = format_missing_dependency(
        "modules/infineon/TARGET_CYT4BB7/config/GeneratedSource"
    )
    assert "GeneratedSource" in message
    assert "git submodule update --init" in message
    assert "TARGET_CYT4BB7" in message
```

- [ ] **Step 2: Run the focused test and verify it fails because the helper is absent.**

```bash
python3 -m unittest Tools.scripts.tests.test_cyt4bb7_tools -v
```

Expected: import or attribute failure for `format_missing_dependency`.

- [ ] **Step 3: Add the dependency helper and early checks.**

Before the existing `cfg.srcnode.find_dir(...).abspath()` calls, check these exact paths:

```python
required = "modules/infineon/TARGET_CYT4BB7/config/GeneratedSource"
node = cfg.srcnode.find_dir(required)
if node is None:
    raise Errors.ConfigurationError(format_missing_dependency(required))
```

The helper message must state that the build does not initialize submodules automatically and show:

```text
git submodule update --init modules/infineon/TARGET_CYT4BB7
```

- [ ] **Step 4: Run the focused test and the board configure command.**

```bash
python3 -m unittest Tools.scripts.tests.test_cyt4bb7_tools -v
./waf configure --board UluluDriveDayun
```

Expected: unit test passes; board configure exits nonzero with the new actionable message while the submodule remains untouched.

- [ ] **Step 5: Commit the scoped change.**

```bash
git add Tools/ardupilotwaf/infineon.py Tools/scripts/tests/test_cyt4bb7_tools.py
git commit -m "build: clarify missing CYT4BB7 generated sources"
```

### Task 2: Implement the read-only multicore and DAPLink preflight

**Files:**
- Create: `Tools/scripts/check_cyt4bb7_multicore.py`
- Modify: `Tools/scripts/tests/test_cyt4bb7_tools.py`

**Interfaces:**
- Consumes: `--repo-root`, optional `--probe`, optional `--json`.
- Produces: status records for dependency state, CM7/CM0+ source/linker state, image-packaging state, ARM tools, DAPLink probe, and pyOCD/OpenOCD target support.

- [ ] **Step 1: Define the status model and tests.**

The script must expose importable functions:

```python
def inspect_repository(repo_root: pathlib.Path) -> dict:
    """Return dependency/core/artifact status without changing files."""

def classify_build_status(status: dict) -> str:
    """Return CM7_ONLY, DUAL_CORE_READY, or BLOCKED."""

def build_probe_commands(probe: str) -> list[list[str]]:
    """Return commands only; do not execute them."""
```

Tests must cover:

```python
assert classify_build_status({"generated_source": False}) == "BLOCKED"
assert classify_build_status({"generated_source": True, "cm7": True,
                              "cm0plus": False}) == "CM7_ONLY"
assert classify_build_status({"generated_source": True, "cm7": True,
                              "cm0plus": True, "combined_image": True}) == "DUAL_CORE_READY"
assert build_probe_commands("pyocd") == [["pyocd", "list"], ["pyocd", "list", "--targets"]]
```

- [ ] **Step 2: Implement deterministic repository inspection.**

Inspect, without importing SDK code or modifying the repository:

- `modules/infineon/TARGET_CYT4BB7/config/GeneratedSource`
- `modules/infineon/TARGET_CYT4BB7/COMPONENT_CM7`
- `modules/infineon/TARGET_CYT4BB7/COMPONENT_CM7/TOOLCHAIN_GCC_ARM/linker_d.ld`
- CM0+ directory candidates under `TARGET_CYT4BB7` matching `CM0`, `CM0P`, or `CM0+`
- linker files matching `*.ld`
- image/packaging files matching `*.hex`, `*.bin`, `*.elf`, `*.cyacd`, `*.pack`, and explicit manifest names

The inspector must distinguish `missing`, `present`, and `unknown`; it must never infer CM0+ support merely from the CYT4BB7 device name.

- [ ] **Step 3: Add tool and probe detection.**

Use `shutil.which()` for:

```text
arm-none-eabi-gcc
arm-none-eabi-objcopy
pyocd
openocd
```

When `--probe` is supplied, execute only the selected non-writing discovery command with `subprocess.run(..., check=False, capture_output=True, text=True)`. Do not run `program`, `flash`, `erase`, `reset`, or `load` from this script.

- [ ] **Step 4: Implement text and JSON output.**

Default output must include stable lines such as:

```text
GeneratedSource: MISSING
CM7 artifacts: PRESENT
CM0+ artifacts: MISSING
Combined image: UNKNOWN
Build status: BLOCKED
pyOCD: FOUND
DAPLink probe: NOT TESTED
```

`--json` must emit the same facts as a JSON object and return exit code `0` for a completed inspection, even when the result is `BLOCKED` or `CM7_ONLY`; infrastructure failure is reserved for malformed arguments or an unreadable repository root.

- [ ] **Step 5: Run tests and the real preflight.**

```bash
python3 -m unittest Tools.scripts.tests.test_cyt4bb7_tools -v
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root .
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root . --json
```

Expected current result: `GeneratedSource: MISSING`, `Build status: BLOCKED`, no files changed.

- [ ] **Step 6: Commit the read-only preflight.**

```bash
git add Tools/scripts/check_cyt4bb7_multicore.py Tools/scripts/tests/test_cyt4bb7_tools.py
git commit -m "build: add CYT4BB7 multicore preflight"
```

### Task 3: Add guarded DAPLink flashing preparation

**Files:**
- Create: `Tools/scripts/flash_cyt4bb7_daplink.py`
- Modify: `Tools/scripts/tests/test_cyt4bb7_tools.py`

**Interfaces:**
- Consumes: `--probe {pyocd,openocd}`, `--image`, optional `--target`, optional `--address`, and `--dry-run`.
- Produces: a validated command preview or a refusal; no write operation unless all required explicit inputs are present and the user supplies `--write`.

- [ ] **Step 1: Add refusal tests before implementation.**

```python
def test_flash_requires_dry_run_or_write_flag():
    result = validate_flash_request(image="firmware.hex", target=None,
                                    address=None, write=False)
    assert result.ok is False
    assert "--write" in result.reason

def test_flash_rejects_missing_target_and_address():
    result = validate_flash_request(image="firmware.hex", target=None,
                                    address=None, write=True)
    assert result.ok is False
    assert "target" in result.reason.lower()
```

- [ ] **Step 2: Implement strict request validation.**

The script must reject:

- missing image or unreadable image;
- missing explicit target configuration;
- missing explicit flash address;
- missing selected executable;
- `--write` without a successful non-writing probe;
- `.elf`/`.hex` ambiguity when no explicit `--image-format` is supplied;
- any request that asks the script to invent CM0+ content or combine images.

The default mode must be dry-run. The command must print a warning that DAPLink support and the CYT4BB7 flash algorithm are not proven by tool presence alone.

- [ ] **Step 3: Implement command construction only for explicit configurations.**

For pyOCD, construct a command from explicit values without assuming a target name:

```text
pyocd load --target <explicit-target> --base-address <explicit-address> <image>
```

For OpenOCD, construct a command only when the caller supplies an existing interface config and target config:

```text
openocd -f <interface-cfg> -f <target-cfg> -c "program <image> verify reset exit"
```

The script must not execute either command in dry-run mode. In write mode it must require `--confirm-cyt4bb7-write` in addition to `--write` and a successful probe.

- [ ] **Step 4: Run tests and safe command previews.**

```bash
python3 -m unittest Tools.scripts.tests.test_cyt4bb7_tools -v
python3 Tools/scripts/flash_cyt4bb7_daplink.py --help
python3 Tools/scripts/flash_cyt4bb7_daplink.py --probe pyocd --image missing.hex
```

Expected: help succeeds; missing image and incomplete target data are refused; no write/reset/erase command executes.

- [ ] **Step 5: Commit the guarded wrapper.**

```bash
git add Tools/scripts/flash_cyt4bb7_daplink.py Tools/scripts/tests/test_cyt4bb7_tools.py
git commit -m "build: add guarded CYT4BB7 DAPLink flashing wrapper"
```

### Task 4: Document build, probe, and flashing boundaries

**Files:**
- Create: `docs/CYT4BB7_MULTICORE_BUILD_FLASH.md`

**Interfaces:**
- Consumes: output names and status values from Tasks 1-3.
- Produces: operator instructions for dependency initialization, CM7/CM0+ classification, DAPLink probing, dry-run flashing, and hardware validation.

- [ ] **Step 1: Document the current verified state.**

Include exact commands and expected current results:

```bash
git submodule update --init modules/infineon/TARGET_CYT4BB7
./waf configure --board UluluDriveDayun
./waf copter
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root . --probe pyocd
```

State that the current checkout is blocked until `GeneratedSource` is present, and that existing ArduPilot flags currently describe a CM7 application path rather than proof of a CM7+CM0+ product image.

- [ ] **Step 2: Document DAPLink/OpenOCD/pyOCD prerequisites.**

Explain that DAPLink supplies CMSIS-DAP transport but a matching CYT4BB7 target definition and Flash algorithm are still required. Include probe-only commands:

```bash
pyocd list
pyocd list --targets
openocd -f interface/cmsis-dap.cfg -f target/<validated-cyt4bb7>.cfg
```

Explicitly prohibit using `Tools/scripts/uploader.py` unless a future CYT4BB7 bootloader protocol is implemented and verified.

- [ ] **Step 3: Document multicore acceptance evidence.**

Define the only acceptable claims:

```text
CM7_ONLY: CM7 source + linker + ArduPilot application build available.
DUAL_CORE_READY: CM7 and CM0+ inputs + startup/linker metadata + packaging rule available.
FLASHABLE: DUAL_CORE_READY + validated DAPLink target/algorithm + successful probe.
```

- [ ] **Step 4: Run Markdown and repository checks.**

```bash
git diff --check
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root . --json
```

- [ ] **Step 5: Commit the documentation.**

```bash
git add docs/CYT4BB7_MULTICORE_BUILD_FLASH.md
git commit -m "docs: define CYT4BB7 multicore DAPLink workflow"
```

### Task 5: Final integrated verification

**Files:**
- Modify: `docs/AM32_KISS_TTL_IMPLEMENTATION.md` only if the new status changes an existing claim.
- Create: `.superpowers/sdd/2026-10-14-cyt4bb7-multicore-daplink/final-report.md`

**Interfaces:**
- Consumes: commits and command results from Tasks 1-4.
- Produces: evidence-backed final report; no product code changes beyond the scoped Waf diagnostic.

- [ ] **Step 1: Run the Python unit tests and preflight.**

```bash
python3 -m unittest Tools.scripts.tests.test_cyt4bb7_tools -v
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root .
python3 Tools/scripts/check_cyt4bb7_multicore.py --repo-root . --probe pyocd
```

- [ ] **Step 2: Run ArduPilot checks.**

```bash
python3 Tools/scripts/check_serial_protocol_gates.py
./waf configure --board sitl
./waf configure --board UluluDriveDayun
git diff --check
```

Record that SITL is expected to pass and the target board remains blocked if the dependency is absent.

- [ ] **Step 3: Verify no unsafe write occurred.**

Inspect command logs and test mocks to ensure no `program`, `flash`, `erase`, `reset`, or `load` operation was executed by preflight or dry-run tests.

- [ ] **Step 4: Write the final report and commit it.**

```bash
git add .superpowers/sdd/2026-10-14-cyt4bb7-multicore-daplink/final-report.md
git commit -m "test: verify CYT4BB7 multicore DAPLink readiness"
```

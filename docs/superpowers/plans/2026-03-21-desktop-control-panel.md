# Desktop Control Panel Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure-C native desktop control panel for the HIS project using `raylib + raygui`, while keeping the existing console executable intact.

**Architecture:** Create a second executable `his_desktop` with a small desktop shell, page-state modules, and adapter functions that reuse the existing `AuthService`, `PatientService`, `RegistrationService`, and `PharmacyService`. Vendor official `raylib` source under `third_party/raylib` and `raygui.h` under `third_party/` so the MinGW 32-bit toolchain can build without relying on a preinstalled package. Keep UI rendering/state separate from domain/service code so the desktop layer remains thin and testable.

**Tech Stack:** C11, CMake, MinGW, raylib, raygui, existing HIS service layer, CTest

---

## Chunk 1: Dependency Bootstrap

### Task 1: Make `raylib + raygui` available to the build

**Files:**
- Modify: `CMakeLists.txt`
- Create: `third_party/raylib/`
- Create: `third_party/raygui.h`
- Inspect: local MinGW include/lib directories

- [ ] **Step 1: Detect local availability of `raylib`**

Run: `Get-ChildItem C:\\msys64\\mingw32\\include,C:\\msys64\\mingw32\\lib -Recurse | Select-String -Pattern "raylib|raygui"`
Expected: either existing headers/libs are found, or nothing is found and vendoring/install is required

- [ ] **Step 2: Add a failing desktop target**

Modify `CMakeLists.txt` to define `his_desktop` that references a placeholder desktop main file.

Run: `cmake --build build`
Expected: FAIL because desktop source files or GUI dependency linkage are still missing

- [ ] **Step 3: Land the dependency path**

Implementation target:
- Prefer system `raylib` linkage if already present
- If MinGW 32-bit package is unavailable, vendor official `raylib` source into `third_party/raylib`
- Vendor `raygui.h` into `third_party/`
- Keep dependency configuration isolated to `his_desktop` / `desktop_core`

- [ ] **Step 4: Re-run build to verify dependency wiring**

Run: `cmake --build build`
Expected: desktop target gets past dependency resolution, even if placeholder app code still fails

## Chunk 2: Desktop Shell And Theme

### Task 2: Create a launchable desktop shell

**Files:**
- Create: `include/ui/DesktopApp.h`
- Create: `src/ui/DesktopApp.c`
- Create: `include/ui/DesktopTheme.h`
- Create: `src/ui/DesktopTheme.c`
- Create: `src/desktop/main.c`
- Modify: `CMakeLists.txt`
- Test: `tests/test_desktop_state.c`
- Test registration: add `add_executable()` and `add_test()` entries for `test_desktop_state`

- [ ] **Step 1: Write failing desktop-state tests**

Cover:
- default page is login
- login success transitions to dashboard
- logout returns to login

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build`
Expected: FAIL because desktop state types/functions are not implemented

- [ ] **Step 3: Implement minimal shell and theme**

Implementation target:
- desktop window bootstrap
- app/page enums
- global desktop session state
- color/spacing/theme constants
- login page placeholder
- dashboard placeholder

- [ ] **Step 4: Run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

## Chunk 3: Desktop Adapters And Session Integration

### Task 3: Bridge desktop UI state to existing services

**Files:**
- Create: `include/ui/DesktopAdapters.h`
- Create: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `include/repository/DispenseRecordRepository.h`
- Modify: `src/repository/DispenseRecordRepository.c`
- Test: `tests/test_desktop_adapters.c`
- Test registration: add `add_executable()` and `add_test()` entries for `test_desktop_adapters`

- [ ] **Step 1: Write failing adapter tests**

Cover:
- desktop login delegates to `AuthService`
- patient query delegates to `PatientService`
- registration submit delegates to `RegistrationService`
- dispense history query delegates to patient-scoped pharmacy query

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_state"`
Expected: FAIL in the new adapter assertions

- [ ] **Step 3: Implement adapter layer**

Implementation target:
- thin wrappers returning desktop-friendly structs/messages
- no direct txt I/O from desktop rendering code
- reuse existing `MenuApplication` init/login/session facilities where practical
- add a backend path for dashboard recent-dispense loading

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_state"`
Expected: PASS

## Chunk 4: MVP Pages

### Task 4: Implement login and dashboard pages

**Files:**
- Create: `include/ui/DesktopPages.h`
- Create: `src/ui/DesktopPages.c`
- Modify: `src/ui/DesktopApp.c`
- Test: `tests/test_desktop_state.c`

- [ ] **Step 1: Add failing tests for login-page and dashboard state**

Cover:
- login form validation
- dashboard refresh state
- logout button action

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL in the new desktop page-state assertions

- [ ] **Step 3: Implement login and dashboard rendering**

Implementation target:
- login form with user id/password fields
- dashboard cards for patients, registrations, inpatients, low stock
- recent registrations and recent dispenses preview blocks

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

### Task 5: Implement patient, registration, and dispense pages

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `src/ui/DesktopApp.c`
- Modify: `include/ui/DesktopPages.h`
- Test: `tests/test_desktop_adapters.c`

- [ ] **Step 1: Add failing tests for page actions**

Cover:
- patient search criteria validation
- registration submit validation
- dispense-history patient scoping

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_state"`
Expected: FAIL in the new page-action assertions

- [ ] **Step 3: Implement the three MVP pages**

Implementation target:
- Patient page: search bar, list, selected patient details
- Registration page: form submit + result panel
- Dispense history page: patient-scoped query table

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_state|test_menu_application"`
Expected: PASS

## Chunk 5: Final Build And Smoke

### Task 6: Verify desktop app compiles and launches

**Files:**
- Inspect only: all desktop files and `CMakeLists.txt`

- [ ] **Step 1: Build full project**

Run: `cmake --build build`
Expected: PASS

- [ ] **Step 2: Run full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 3: Run desktop smoke**

Run: `.\\build\\his_desktop.exe`
Expected: window opens to login screen without crashing

- [ ] **Step 4: Record deferred items**

Defer explicitly:
- richer charts and animation
- doctor/ward/pharmacy operation pages beyond MVP
- automated GUI interaction tests
- cross-platform packaging

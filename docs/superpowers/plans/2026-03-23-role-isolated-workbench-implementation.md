# Role-Isolated Desktop Workbench Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the native desktop frontend into seven role-isolated workbenches with strict role routing, role-specific home pages, dedicated side navigation, and enforceable page/data boundaries.

**Architecture:** Keep `his_desktop` as a single-window `raylib + raygui` desktop shell, but move from one global page enum to a `workbench + local page` model. Reuse `DesktopAdapters` as the bridge to `MenuApplication`, add a workbench registry/common UI layer, and split role UI into focused modules so access boundaries are enforced structurally instead of by ad-hoc `if role` checks inside a monolithic page file.

**Tech Stack:** C11, CMake, MinGW/Ninja, raylib, raygui, existing MenuApplication/service layer, CTest

---

## File Structure

### Core desktop files to modify

- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

### New workbench infrastructure

- Create: `include/ui/DesktopWorkbench.h`
- Create: `src/ui/DesktopWorkbench.c`
- Create: `include/ui/DesktopWorkbenchCommon.h`
- Create: `src/ui/DesktopWorkbenchCommon.c`

### New role workbench modules

- Create: `include/ui/workbench/AdminWorkbench.h`
- Create: `src/ui/workbench/AdminWorkbench.c`
- Create: `include/ui/workbench/ClerkWorkbench.h`
- Create: `src/ui/workbench/ClerkWorkbench.c`
- Create: `include/ui/workbench/DoctorWorkbench.h`
- Create: `src/ui/workbench/DoctorWorkbench.c`
- Create: `include/ui/workbench/PatientWorkbench.h`
- Create: `src/ui/workbench/PatientWorkbench.c`
- Create: `include/ui/workbench/InpatientRegistrarWorkbench.h`
- Create: `src/ui/workbench/InpatientRegistrarWorkbench.c`
- Create: `include/ui/workbench/WardManagerWorkbench.h`
- Create: `src/ui/workbench/WardManagerWorkbench.c`
- Create: `include/ui/workbench/PharmacyWorkbench.h`
- Create: `src/ui/workbench/PharmacyWorkbench.c`

### Existing business entrypoints to inspect during implementation

- Inspect: `include/ui/MenuApplication.h`
- Inspect: `src/ui/MenuApplication.c`
- Inspect: `include/service/AuthService.h`
- Inspect: `include/service/RegistrationService.h`
- Inspect: `include/service/MedicalRecordService.h`
- Inspect: `include/service/InpatientService.h`
- Inspect: `include/service/PharmacyService.h`

### Tests to modify or add

- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `tests/test_desktop_workflows.c`
- Create: `tests/test_desktop_workbench_registry.c`
- Create: `tests/test_desktop_role_guards.c`

### Reference documents

- Inspect: `docs/superpowers/specs/2026-03-23-role-isolated-workbench-design.md`
- Inspect: `docs/superpowers/specs/2026-03-21-desktop-control-panel-design.md`
- Inspect: `docs/superpowers/plans/2026-03-22-desktop-frontend-completion.md`

## Chunk 1: Workbench Identity And State Model

### Task 1: Introduce workbench ids, local page ids, and strict home routing

**Files:**
- Create: `include/ui/DesktopWorkbench.h`
- Create: `src/ui/DesktopWorkbench.c`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `tests/test_desktop_state.c`
- Create: `tests/test_desktop_workbench_registry.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for the new routing model**

Add tests that assert:

```c
assert(DesktopWorkbench_for_role(USER_ROLE_ADMIN) == DESKTOP_WORKBENCH_ADMIN);
assert(DesktopWorkbench_home_page(DESKTOP_WORKBENCH_PATIENT) == PATIENT_PAGE_HOME);
assert(DesktopAppState_current_workbench(&state) == DESKTOP_WORKBENCH_DOCTOR);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake -S . -B build && cmake --build build --target test_desktop_state test_desktop_workbench_registry && ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workbench_registry"`
Expected: FAIL because workbench ids, registry helpers, and local page routing do not exist yet.

- [ ] **Step 3: Add the workbench identity model**

Implement:
- `DesktopWorkbenchId`
- per-role local page enums or integer page keys in `DesktopWorkbench.h`
- helpers for `role -> workbench`
- helpers for `workbench -> default home page`
- updated `DesktopAppState` fields for `current_workbench` and local page selection

- [ ] **Step 4: Update state initialization and login landing**

Change `DesktopAppState_init`, `DesktopAppState_login_success`, `DesktopAppState_logout`, and helper APIs so:
- login always lands on the role’s workbench home page
- state no longer assumes one shared dashboard page for all roles
- logout clears the mounted workbench state cleanly

- [ ] **Step 5: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workbench_registry"`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/ui/DesktopApp.h src/ui/DesktopApp.c include/ui/DesktopWorkbench.h src/ui/DesktopWorkbench.c tests/test_desktop_state.c tests/test_desktop_workbench_registry.c CMakeLists.txt
git commit -m "refactor: add desktop workbench identity model"
```

### Task 2: Enforce strict login role matching

**Files:**
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `tests/test_desktop_adapters.c`

- [ ] **Step 1: Write failing tests for role mismatch rejection**

Replace the current fallback behavior with explicit failure expectations:

```c
result = DesktopAdapters_login(&application, "PATD001", "desktop-pass", USER_ROLE_DOCTOR, &user);
assert(result.success == 0);
assert(strstr(result.message, "role") != 0);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters && ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: FAIL because `DesktopAdapters_login` still falls back to `USER_ROLE_UNKNOWN`.

- [ ] **Step 3: Remove fallback login behavior**

Implementation target:
- keep authentication strict
- return a role mismatch failure instead of silently downgrading into another role
- preserve the successful login path for correct credentials and matching roles

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopAdapters.h src/ui/DesktopAdapters.c tests/test_desktop_adapters.c
git commit -m "fix: enforce strict desktop login role matching"
```

## Chunk 2: Shell, Registry, And Page Guard Rails

### Task 3: Split shared desktop shell from role workbench rendering

**Files:**
- Create: `include/ui/DesktopWorkbenchCommon.h`
- Create: `src/ui/DesktopWorkbenchCommon.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing smoke-level tests for shell rendering helpers**

Add tests that cover helper-level contracts such as:

```c
assert(DesktopWorkbenchCommon_nav_item_count(items) == expected_count);
assert(DesktopWorkbenchCommon_role_color(DESKTOP_WORKBENCH_PHARMACY) != 0);
```

Use a new helper test file only if the logic cannot fit naturally in `tests/test_desktop_state.c`.

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state test_desktop_workbench_registry && ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workbench_registry"`
Expected: FAIL because shared shell/common helper APIs are missing.

- [ ] **Step 3: Add shared workbench UI primitives**

Implement reusable helpers for:
- role title/header area
- metric cards
- quick action cards
- section headers
- output/empty state panels
- role-aware sidebar item drawing

Keep them generic and role-agnostic.

- [ ] **Step 4: Reduce `DesktopPages.c` to shell orchestration**

Move `DesktopPages_draw` toward this responsibility split:
- login page
- topbar/message bar
- sidebar shell
- dispatch into current workbench module

Do not leave full business page implementations in the dispatcher.

- [ ] **Step 5: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workbench_registry"`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/ui/DesktopPages.h src/ui/DesktopPages.c include/ui/DesktopWorkbenchCommon.h src/ui/DesktopWorkbenchCommon.c include/ui/DesktopTheme.h src/ui/DesktopTheme.c CMakeLists.txt
git commit -m "refactor: split desktop shell from workbench rendering"
```

### Task 4: Add role-aware navigation and invalid-page guards

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/DesktopPages.c`
- Create: `tests/test_desktop_role_guards.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for page visibility and guard behavior**

Add assertions such as:

```c
assert(DesktopWorkbench_page_visible(DESKTOP_WORKBENCH_DOCTOR, DOCTOR_PAGE_HOME) == 1);
assert(DesktopWorkbench_page_visible(DESKTOP_WORKBENCH_DOCTOR, PATIENT_PAGE_HOME) == 0);
assert(DesktopApp_guard_page(&state, PATIENT_PAGE_HOME) == DOCTOR_PAGE_HOME);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_role_guards && ctest --test-dir build --output-on-failure -R test_desktop_role_guards`
Expected: FAIL because page visibility and redirect guards do not exist.

- [ ] **Step 3: Implement page guard helpers**

Implementation target:
- role-aware sidebar only shows current workbench pages
- invalid page selections are redirected to the current workbench home page
- message bar can display a consistent “无权访问该页面” style error

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_role_guards`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopApp.h src/ui/DesktopApp.c src/ui/DesktopPages.c tests/test_desktop_role_guards.c CMakeLists.txt
git commit -m "feat: add desktop role page guards"
```

## Chunk 3: Role Home Pages And Dedicated Navigation

### Task 5: Implement admin, clerk, and patient workbench modules

**Files:**
- Create: `include/ui/workbench/AdminWorkbench.h`
- Create: `src/ui/workbench/AdminWorkbench.c`
- Create: `include/ui/workbench/ClerkWorkbench.h`
- Create: `src/ui/workbench/ClerkWorkbench.c`
- Create: `include/ui/workbench/PatientWorkbench.h`
- Create: `src/ui/workbench/PatientWorkbench.c`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_workflows.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for dedicated landing pages**

Cover:
- admin lands on admin home, not generic dashboard
- clerk lands on clerk home with clerk nav
- patient lands on self-service home with patient-only nav

Example assertions:

```c
assert(state.current_workbench == DESKTOP_WORKBENCH_CLERK);
assert(state.current_page == CLERK_PAGE_HOME);
assert(DesktopWorkbench_page_visible(DESKTOP_WORKBENCH_PATIENT, CLERK_PAGE_REGISTRATION) == 0);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state test_desktop_workflows && ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workflows"`
Expected: FAIL because these workbench modules and home pages do not exist.

- [ ] **Step 3: Implement admin workbench**

Implementation target:
- role-specific home page
- admin-only nav
- metric cards for global overview
- quick entries for patient/doctor/record/system summary
- no direct operational entry cards

- [ ] **Step 4: Implement clerk workbench**

Implementation target:
- reception-style home page
- dedicated nav for patient add/update/query + registration create/query/cancel
- quick actions that jump into clerk-only pages

- [ ] **Step 5: Implement patient workbench**

Implementation target:
- self-service home page
- patient-only nav
- personal status cards and next-step reminders
- no cross-patient search affordance

- [ ] **Step 6: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workflows"`
Expected: PASS for the new routing/navigation assertions

- [ ] **Step 7: Commit**

```bash
git add include/ui/workbench/AdminWorkbench.h src/ui/workbench/AdminWorkbench.c include/ui/workbench/ClerkWorkbench.h src/ui/workbench/ClerkWorkbench.c include/ui/workbench/PatientWorkbench.h src/ui/workbench/PatientWorkbench.c include/ui/DesktopApp.h src/ui/DesktopApp.c src/ui/DesktopPages.c tests/test_desktop_state.c tests/test_desktop_workflows.c CMakeLists.txt
git commit -m "feat: add admin clerk and patient workbenches"
```

### Task 6: Implement doctor, inpatient registrar, ward manager, and pharmacy workbench modules

**Files:**
- Create: `include/ui/workbench/DoctorWorkbench.h`
- Create: `src/ui/workbench/DoctorWorkbench.c`
- Create: `include/ui/workbench/InpatientRegistrarWorkbench.h`
- Create: `src/ui/workbench/InpatientRegistrarWorkbench.c`
- Create: `include/ui/workbench/WardManagerWorkbench.h`
- Create: `src/ui/workbench/WardManagerWorkbench.c`
- Create: `include/ui/workbench/PharmacyWorkbench.h`
- Create: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_workflows.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for the remaining role homes**

Cover:
- doctor lands on doctor home
- inpatient registrar lands on registrar home
- ward manager lands on ward home
- pharmacy lands on pharmacy home
- registrar and ward manager no longer share one page id

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state test_desktop_workflows && ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workflows"`
Expected: FAIL because current app still uses shared inpatient/pharmacy/doctor page assumptions.

- [ ] **Step 3: Implement doctor workbench**

Implementation target:
- dedicated doctor home
- doctor-only nav for queue/history/visit/exam
- doctor queue and record tools split across doctor-local pages

- [ ] **Step 4: Implement inpatient registrar and ward manager workbenches**

Implementation target:
- registrar workbench only for admit/discharge/status
- ward manager workbench only for ward/bed/inpatient lookup/transfer/discharge-check
- no shared “住院管理” page between the two roles

- [ ] **Step 5: Implement pharmacy workbench**

Implementation target:
- dedicated home and nav
- split medicine master, stock, restock, dispense, low-stock views
- pharmacy-specific overview cards

- [ ] **Step 6: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_workflows"`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add include/ui/workbench/DoctorWorkbench.h src/ui/workbench/DoctorWorkbench.c include/ui/workbench/InpatientRegistrarWorkbench.h src/ui/workbench/InpatientRegistrarWorkbench.c include/ui/workbench/WardManagerWorkbench.h src/ui/workbench/WardManagerWorkbench.c include/ui/workbench/PharmacyWorkbench.h src/ui/workbench/PharmacyWorkbench.c include/ui/DesktopApp.h src/ui/DesktopApp.c src/ui/DesktopPages.c tests/test_desktop_state.c tests/test_desktop_workflows.c CMakeLists.txt
git commit -m "feat: add isolated doctor inpatient ward and pharmacy workbenches"
```

## Chunk 4: Role-Specific Flows, Ownership, And UI Extraction

### Task 7: Extract clerk and patient operational pages from shared page state

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/workbench/ClerkWorkbench.c`
- Modify: `src/ui/workbench/PatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests for separated clerk/patient flows**

Cover:
- clerk can add/query/update/register/cancel inside clerk-local navigation
- patient can only browse self-service pages and cannot enter clerk registration flow

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on the new role-isolated flow assertions.

- [ ] **Step 3: Split clerk and patient page state**

Implementation target:
- clerk-specific form state no longer lives in a generic shared registration page
- patient self-service state is read-only or identity-bound where appropriate
- patient workbench uses distinct cards/sections for registration, visit, exam, admission, dispense history

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopApp.h src/ui/DesktopApp.c src/ui/workbench/ClerkWorkbench.c src/ui/workbench/PatientWorkbench.c tests/test_desktop_workflows.c
git commit -m "refactor: isolate clerk and patient desktop flows"
```

### Task 8: Extract doctor, registrar, ward, and pharmacy operational pages from shared page state

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/workbench/DoctorWorkbench.c`
- Modify: `src/ui/workbench/InpatientRegistrarWorkbench.c`
- Modify: `src/ui/workbench/WardManagerWorkbench.c`
- Modify: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests for separated operational workbenches**

Cover:
- doctor can complete queue/history/visit/exam flow only inside doctor pages
- inpatient registrar cannot access transfer/discharge-check pages
- ward manager cannot access admit/discharge pages
- pharmacy flow is isolated to pharmacy pages

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on workbench separation assertions.

- [ ] **Step 3: Split the remaining workbench-local state**

Implementation target:
- doctor workbench owns doctor-local page state
- registrar workbench owns admit/discharge/status state
- ward manager workbench owns ward/bed/transfer/discharge-check state
- pharmacy workbench owns medicine/inventory/dispense state

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopApp.h src/ui/DesktopApp.c src/ui/workbench/DoctorWorkbench.c src/ui/workbench/InpatientRegistrarWorkbench.c src/ui/workbench/WardManagerWorkbench.c src/ui/workbench/PharmacyWorkbench.c tests/test_desktop_workflows.c
git commit -m "refactor: isolate desktop workbench operational state"
```

### Task 9: Enforce ownership and self-binding in desktop workflows

**Files:**
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `src/ui/workbench/DoctorWorkbench.c`
- Modify: `src/ui/workbench/PatientWorkbench.c`
- Modify: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing tests for role-bound identity behavior**

Add or update tests covering:

```c
assert(strcmp(bound_doctor_id, current_user.user_id) == 0);
assert(strcmp(bound_pharmacist_id, current_user.user_id) == 0);
assert(strstr(patient_output, "PAT0002") == 0); /* other patient hidden */
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters test_desktop_workflows && ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_workflows"`
Expected: FAIL because the current desktop layer still allows looser identity entry behavior.

- [ ] **Step 3: Implement role-bound identity defaults and ownership guards**

Implementation target:
- doctor pages auto-fill and lock `doctor_id` to the logged-in doctor
- patient pages auto-bind `patient_id` to the logged-in patient
- pharmacy pages auto-fill and lock `pharmacist_id` to the logged-in pharmacist
- non-owned patient history/dispense queries fail fast with a clear message

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_workflows"`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopAdapters.h src/ui/DesktopAdapters.c src/ui/workbench/DoctorWorkbench.c src/ui/workbench/PatientWorkbench.c src/ui/workbench/PharmacyWorkbench.c tests/test_desktop_adapters.c tests/test_desktop_workflows.c
git commit -m "feat: enforce desktop role ownership and self binding"
```

## Chunk 5: Final Verification, Docs, And Release Flow

### Task 10: Wire new sources and tests into the build graph

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new workbench sources to `desktop_core`**

Include every new shared and role workbench source file in `desktop_core`.

- [ ] **Step 2: Add new tests to CTest**

Add targets for:
- `test_desktop_workbench_registry`
- `test_desktop_role_guards`

Keep the existing `-UNDEBUG` behavior for all new test targets.

- [ ] **Step 3: Verify build graph**

Run: `cmake -S . -B build && cmake --build build`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: wire desktop workbench modules and tests"
```

### Task 11: Refresh README and packaging guidance

**Files:**
- Modify: `README.md`
- Modify: `scripts/package-release.ps1` only if screenshots/version text must be updated for new workbench naming

- [ ] **Step 1: Document the role-isolated desktop workbench**

Update README to describe:
- seven isolated role workbenches
- strict role login matching
- patient self-service workbench
- split inpatient registrar vs ward manager workbenches

- [ ] **Step 2: Verify packaging docs still match actual output**

Run: `powershell -ExecutionPolicy Bypass -File .\\scripts\\package-release.ps1 -SkipTests`
Expected: PASS and package output still lands in `dist/`

- [ ] **Step 3: Commit**

```bash
git add README.md scripts/package-release.ps1
git commit -m "docs: describe isolated desktop workbench release"
```

### Task 12: Run full verification before handoff

**Files:**
- No code changes required unless failures are found

- [ ] **Step 1: Run focused desktop test suite**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_adapters|test_desktop_workflows|test_desktop_workbench_registry|test_desktop_role_guards"`
Expected: PASS

- [ ] **Step 2: Run full project test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 3: Run desktop smoke**

Run: `.\\build\\his_desktop.exe --smoke`
Expected: PASS

- [ ] **Step 4: Rebuild portable release**

Run: `powershell -ExecutionPolicy Bypass -File .\\scripts\\package-release.ps1`
Expected: PASS and a new `dist\\lightweight-his-portable-v0.2.1-win32.zip`

- [ ] **Step 5: Inspect git status and finalize**

Run: `git status --short`
Expected: only intentional implementation changes remain

- [ ] **Step 6: Final commit**

```bash
git add .
git commit -m "feat: ship role-isolated desktop workbenches"
```

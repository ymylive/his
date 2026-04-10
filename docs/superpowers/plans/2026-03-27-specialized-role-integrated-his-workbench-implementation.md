# Specialized Role Integrated HIS Workbench Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the native desktop frontend into seven specialized role workbenches with stable Chinese rendering, non-overlapping responsive layouts, end-to-end cross-role workflows, and demo-ready simulated data.

**Architecture:** Keep the existing single-window `raylib + raygui` desktop shell and current `MenuApplication/service/repository` business stack. Implement in four waves: shared shell/layout and freshness infrastructure first, then public application/adapter contracts, then role workbench workflow completion, then simulation data and end-to-end verification. Use parallel `gpt-5.4` subagents with non-overlapping file ownership.

**Tech Stack:** C11, CMake, MinGW/Ninja, raylib, raygui, existing MenuApplication/service layer, CTest, PowerShell packaging scripts

---

## File Structure

### Shared shell and layout files

- Modify: `CMakeLists.txt`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `include/ui/Workbench.h`
- Modify: `src/ui/WorkbenchCommon.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`

### Business bridge files

- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`

### Role workbench files

- Modify: `src/ui/workbench/PatientWorkbench.c`
- Modify: `src/ui/workbench/ClerkWorkbench.c`
- Modify: `src/ui/workbench/DoctorWorkbench.c`
- Modify: `src/ui/workbench/InpatientWorkbench.c`
- Modify: `src/ui/workbench/WardWorkbench.c`
- Modify: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `src/ui/workbench/AdminWorkbench.c`

### Data and release files

- Modify: `README.md`
- Create: `Dockerfile`
- Create: `.dockerignore`
- Create: `docs/INSTALL.md`
- Create: `docs/BUILD_AND_RUN.md`
- Create: `docs/AI_USAGE.md`
- Create: `.github/workflows/portable-release.yml`
- Modify: `data/users.txt`
- Modify: `data/patients.txt`
- Modify: `data/departments.txt`
- Modify: `data/doctors.txt`
- Modify: `data/registrations.txt`
- Modify: `data/visits.txt`
- Modify: `data/examinations.txt`
- Modify: `data/wards.txt`
- Modify: `data/beds.txt`
- Modify: `data/admissions.txt`
- Modify: `data/medicines.txt`
- Modify: `data/dispense_records.txt`
- Inspect: `scripts/package-release.ps1`

### Tests

- Modify: `tests/test_menu_application.c`
- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `tests/test_desktop_workflows.c`
- Create: `tests/test_patient_workbench_flow.c`
- Create: `tests/test_clerk_workbench_flow.c`
- Create: `tests/test_doctor_workbench_flow.c`
- Create: `tests/test_inpatient_ward_workbench_flow.c`
- Create: `tests/test_pharmacy_admin_workbench_flow.c`
- Create: `tests/test_demo_data_integrity.c`
- Create: `tests/test_desktop_layout_rules.c`

### Reference documents

- Inspect: `docs/superpowers/specs/2026-03-27-specialized-role-integrated-his-workbench-design.md`
- Inspect: `docs/superpowers/plans/2026-03-23-role-isolated-workbench-implementation.md`

## Parallel Execution Ownership

Use `gpt-5.4` subagents only. Do not let two subagents edit the same file in the same wave.

### Ownership lanes

- Lane A: shared shell/layout
  - Owns: `include/ui/DesktopApp.h`, `src/ui/DesktopApp.c`, `include/ui/DesktopTheme.h`, `src/ui/DesktopTheme.c`, `include/ui/Workbench.h`, `src/ui/WorkbenchCommon.c`, `include/ui/DesktopPages.h`, `src/ui/DesktopPages.c`, `tests/test_desktop_state.c`, `tests/test_desktop_layout_rules.c`
- Lane B: application and adapter contracts
  - Owns: `include/ui/MenuApplication.h`, `src/ui/MenuApplication.c`, `include/ui/DesktopAdapters.h`, `src/ui/DesktopAdapters.c`, `tests/test_menu_application.c`, `tests/test_desktop_adapters.c`
- Lane C: patient/clerk/doctor workflows
  - Owns: `src/ui/workbench/PatientWorkbench.c`, `src/ui/workbench/ClerkWorkbench.c`, `src/ui/workbench/DoctorWorkbench.c`
  - Owns tests: `tests/test_patient_workbench_flow.c`, `tests/test_clerk_workbench_flow.c`, `tests/test_doctor_workbench_flow.c`
- Lane D: inpatient/ward/pharmacy/admin workflows
  - Owns: `src/ui/workbench/InpatientWorkbench.c`, `src/ui/workbench/WardWorkbench.c`, `src/ui/workbench/PharmacyWorkbench.c`, `src/ui/workbench/AdminWorkbench.c`
  - Owns tests: `tests/test_inpatient_ward_workbench_flow.c`, `tests/test_pharmacy_admin_workbench_flow.c`
- Lane E: data and final verification
  - Owns: `data/*.txt`, `README.md`, `tests/test_demo_data_integrity.c`, final verification commands

### Shared-file sequencing rules

- `DesktopApp.*` is Lane A only during Waves 1-2. Other lanes consume its contracts but do not edit it in parallel.
- `DesktopAdapters.*` and `MenuApplication.*` are Lane B only until all public contracts are in place. Workflow lanes may use new APIs only after Lane B lands.
- `tests/test_desktop_workflows.c` is reserved for the main integrator during each merge checkpoint. Workflow lanes should add temporary focused tests in lane-owned files or hand exact assertions to the integrator.
- `CMakeLists.txt` is reserved for the main integrator except when adding a brand-new test target in the same wave; if needed, batch those edits into one integration pass.
- Integration checkpoints:
  - After Wave 2, the main integrator merges Lane C assertions into `tests/test_desktop_workflows.c`
  - After Wave 3, the main integrator merges Lane D assertions into `tests/test_desktop_workflows.c`
  - No workflow lane edits `tests/test_desktop_workflows.c` directly while another lane is active

## Chunk 1: Shared Shell, Theme, And Layout Guard Rails

### Task 1: Add desktop freshness, minimum window guard, and layout assertions

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `tests/test_desktop_state.c`
- Create: `tests/test_desktop_layout_rules.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for freshness and window constraints**

Add assertions covering:

```c
assert(state.current_page == DESKTOP_PAGE_LOGIN);
assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_CLEAN);
assert(DesktopApp_min_width() >= 1366);
assert(DesktopApp_min_height() >= 768);
DesktopApp_mark_dirty(&state, DESKTOP_PAGE_DASHBOARD);
assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_DIRTY);
DesktopApp_mark_stale(&state, DESKTOP_PAGE_DASHBOARD);
assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_STALE);
DesktopApp_mark_clean(&state, DESKTOP_PAGE_DASHBOARD);
assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_CLEAN);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake -S . -B build && cmake --build build --target test_desktop_state test_desktop_layout_rules && ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_layout_rules"`
Expected: FAIL because freshness enums, page freshness storage, and minimum window helpers do not exist yet.

- [ ] **Step 3: Implement desktop freshness state and minimum window helpers**

Add:
- `DesktopPageFreshness` enum with `CLEAN / DIRTY / STALE`
- per-page freshness storage in `DesktopAppState`
- helpers to mark page dirty/stale/clean
- helpers/tests for “reload failure keeps last successful snapshot” state transitions
- minimum window size constants and init-time enforcement hooks

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_layout_rules"`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopApp.h src/ui/DesktopApp.c tests/test_desktop_state.c tests/test_desktop_layout_rules.c CMakeLists.txt
git commit -m "feat: add desktop freshness and layout guards"
```

### Task 2: Fix Chinese font loading and glyph coverage contract

**Files:**
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `tests/test_desktop_state.c`

- [ ] **Step 1: Write failing tests for font fallback policy**

Add tests that assert:

```c
assert(DesktopTheme_candidate_font_count() >= 3);
assert(DesktopTheme_has_cjk_seed_text() == 1);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL because these helpers and broader fallback policy do not exist.

- [ ] **Step 3: Implement broader CJK font fallback and glyph seeding**

Implementation target:
- expand Windows font candidate list
- keep deterministic fallback order
- expand glyph seed coverage to include current business-domain Chinese strings from desktop workflows and README demo text
- keep startup cost bounded; do not switch to “load all Unicode”

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopTheme.h src/ui/DesktopTheme.c tests/test_desktop_state.c
git commit -m "fix: harden desktop chinese font fallback"
```

### Task 3: Rebuild shared workbench layout helpers to prevent overlapping controls

**Files:**
- Modify: `include/ui/Workbench.h`
- Modify: `src/ui/WorkbenchCommon.c`
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_layout_rules.c`

- [ ] **Step 1: Write failing layout helper tests**

Cover:
- button groups have non-zero spacing
- helper can switch to stacked layout when width is narrow
- info rows and output panels expose wrapped text bounds

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_layout_rules && ctest --test-dir build --output-on-failure -R test_desktop_layout_rules`
Expected: FAIL because responsive layout helpers do not exist.

- [ ] **Step 3: Implement shared responsive helper layer**

Implementation target:
- shared button row/stack helpers
- list/detail/action shell helpers
- wrapped info rows and long-text panel helpers
- top bar and login card spacing use computed widths instead of hard-coded collision-prone offsets

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_layout_rules`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/Workbench.h src/ui/WorkbenchCommon.c src/ui/DesktopPages.c tests/test_desktop_layout_rules.c
git commit -m "refactor: add responsive desktop workbench layout helpers"
```

## Chunk 2: Public Application Contracts And Desktop DTOs

### Task 4: Add missing application APIs for patient self-registration, doctor handoff, and admin maintenance

**Files:**
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `tests/test_menu_application.c`

- [ ] **Step 1: Write failing application tests for missing commands and queries**

Add tests covering:
- patient-bound self-registration wrapper
- department list query
- doctor list by department query
- medicine instruction / usage query
- doctor dispense request creation
- doctor admission request creation
- admin maintenance entrypoints for doctor/department/medicine summary updates

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_menu_application && ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: FAIL because these public APIs are not declared or implemented.

- [ ] **Step 3: Implement minimal public application surface**

Implementation target:
- expose wrappers over existing service logic where possible
- add ownership/session enforcement for patient-bound commands
- avoid unrelated admin CRUD expansion beyond spec minimums

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/MenuApplication.h src/ui/MenuApplication.c tests/test_menu_application.c
git commit -m "feat: add menu application contracts for integrated desktop workflows"
```

### Task 5: Extend desktop adapter contract with workflow DTOs and error policies

**Files:**
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `tests/test_desktop_adapters.c`

- [ ] **Step 1: Write failing adapter tests for new DTOs**

Cover:
- patient home summary DTO
- department/doctor selection DTOs
- doctor queue and handoff DTOs
- dispense request and admission request DTOs
- medicine instruction / usage DTO
- admin overview DTO
- adapter result error kind and snapshot policy behavior for conflict, user error, and system error

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters && ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: FAIL because these DTOs and policy fields do not exist.

- [ ] **Step 3: Implement desktop adapter DTO layer**

Implementation target:
- add structs/enums in `DesktopAdapters.h`
- keep adapters stateless
- normalize `error_kind` and `snapshot_policy`
- map application failures into deterministic desktop-facing results

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/ui/DesktopAdapters.h src/ui/DesktopAdapters.c tests/test_desktop_adapters.c
git commit -m "feat: add integrated desktop workflow adapter dto contracts"
```

### Task 5A: Register lane-owned workflow and demo-data tests in the build graph

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/test_patient_workbench_flow.c`
- Create: `tests/test_clerk_workbench_flow.c`
- Create: `tests/test_doctor_workbench_flow.c`
- Create: `tests/test_inpatient_ward_workbench_flow.c`
- Create: `tests/test_pharmacy_admin_workbench_flow.c`
- Create: `tests/test_demo_data_integrity.c`

- [ ] **Step 1: Create placeholder test files**

Create compilable placeholders for:
- `tests/test_patient_workbench_flow.c`
- `tests/test_clerk_workbench_flow.c`
- `tests/test_doctor_workbench_flow.c`
- `tests/test_inpatient_ward_workbench_flow.c`
- `tests/test_pharmacy_admin_workbench_flow.c`
- `tests/test_demo_data_integrity.c`

Each placeholder should compile, return `0`, and contain a comment stating that later tasks replace it with real failing tests.

- [ ] **Step 2: Add workflow and demo-data test executables and CTest registrations**

Register:
- `test_patient_workbench_flow`
- `test_clerk_workbench_flow`
- `test_doctor_workbench_flow`
- `test_inpatient_ward_workbench_flow`
- `test_pharmacy_admin_workbench_flow`
- `test_demo_data_integrity`

- [ ] **Step 3: Keep new targets inside the shared `HIS_TEST_TARGETS` debug flag list**

Implementation target:
- each new test target gets `target_link_libraries(... PRIVATE desktop_core)` or `his_common` as appropriate
- each target is appended to `HIS_TEST_TARGETS`

- [ ] **Step 4: Verify build graph before workflow chunks rely on it**

Run: `cmake -S . -B build && cmake --build build --target test_patient_workbench_flow test_clerk_workbench_flow test_doctor_workbench_flow test_inpatient_ward_workbench_flow test_pharmacy_admin_workbench_flow test_demo_data_integrity`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/test_patient_workbench_flow.c tests/test_clerk_workbench_flow.c tests/test_doctor_workbench_flow.c tests/test_inpatient_ward_workbench_flow.c tests/test_pharmacy_admin_workbench_flow.c tests/test_demo_data_integrity.c
git commit -m "build: register lane-owned desktop workflow tests"
```

## Chunk 3: Patient, Clerk, And Doctor Workflow Completion

### Task 6: Rebuild patient workbench into a real self-service flow

**Files:**
- Modify: `src/ui/workbench/PatientWorkbench.c`
- Modify: `tests/test_patient_workbench_flow.c`

- [ ] **Step 1: Write failing workflow tests for patient pages**

Cover:
- patient home shows current status and recent flow summary
- patient can self-register by selecting department then doctor
- duplicate pending registration is blocked
- patient can cancel own pending registration
- patient profile, history, admission, and dispense pages preserve self-binding

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_patient_workbench_flow && ctest --test-dir build --output-on-failure -R test_patient_workbench_flow`
Expected: FAIL because current patient workbench is still query-only.

- [ ] **Step 3: Implement patient flow pages**

Implementation target:
- patient home as flow dashboard
- self-registration widget
- own registration/history/admission/dispense/profile subpages
- use shared responsive helpers instead of hard-coded button rows
- consume Lane A-provided freshness helpers and role shell contracts without editing `DesktopApp.*`

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_patient_workbench_flow`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/workbench/PatientWorkbench.c tests/test_patient_workbench_flow.c
git commit -m "feat: rebuild patient self-service desktop workflow"
```

### Task 7: Rebuild clerk workbench into a reception desk flow

**Files:**
- Modify: `src/ui/workbench/ClerkWorkbench.c`
- Modify: `tests/test_clerk_workbench_flow.c`

- [ ] **Step 1: Write failing workflow tests for clerk reception behavior**

Cover:
- clerk landing page shows patient lookup + registration summary
- clerk can add patient, update patient, create registration, query and cancel registration
- successful clerk actions mark the right pages dirty

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_clerk_workbench_flow && ctest --test-dir build --output-on-failure -R test_clerk_workbench_flow`
Expected: FAIL on clerk-specific workflow assertions.

- [ ] **Step 3: Implement clerk reception workbench**

Implementation target:
- left search/recent panel
- center create/update/register panel
- right result/summary panel
- no overlapping button rows at narrow widths

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_clerk_workbench_flow`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/workbench/ClerkWorkbench.c tests/test_clerk_workbench_flow.c
git commit -m "feat: rebuild clerk reception desktop workflow"
```

### Task 8: Rebuild doctor workbench into queue + diagnosis + handoff flow

**Files:**
- Modify: `src/ui/workbench/DoctorWorkbench.c`
- Modify: `tests/test_doctor_workbench_flow.c`

- [ ] **Step 1: Write failing workflow tests for doctor handoff flow**

Cover:
- doctor sees pending queue from patient/clerk registrations
- doctor can create visit, create exam, complete exam
- doctor can create dispense request
- doctor can create admission request
- doctor pages stay bound to logged-in doctor

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_doctor_workbench_flow && ctest --test-dir build --output-on-failure -R test_doctor_workbench_flow`
Expected: FAIL because doctor handoff actions are incomplete.

- [ ] **Step 3: Implement doctor workbench flow**

Implementation target:
- queue list
- current patient summary
- visit/exam/dispense/admission action area
- refresh patients and downstream pages through freshness markers

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_doctor_workbench_flow`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/workbench/DoctorWorkbench.c tests/test_doctor_workbench_flow.c
git commit -m "feat: rebuild doctor desktop queue and handoff workflow"
```

## Chunk 4: Inpatient, Ward, Pharmacy, Admin, And Demo Data

### Task 9: Rebuild inpatient registrar and ward manager workbenches into separate transaction/resource flows

**Files:**
- Modify: `src/ui/workbench/InpatientWorkbench.c`
- Modify: `src/ui/workbench/WardWorkbench.c`
- Modify: `tests/test_inpatient_ward_workbench_flow.c`

- [ ] **Step 1: Write failing workflow tests for inpatient and ward separation**

Cover:
- registrar manages admit/discharge/current inpatient
- ward manager manages bed state, current bed patient, transfer, discharge check
- admission request created by doctor is consumable by registrar

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_inpatient_ward_workbench_flow && ctest --test-dir build --output-on-failure -R test_inpatient_ward_workbench_flow`
Expected: FAIL because current pages share too much state and skip doctor handoff.

- [ ] **Step 3: Implement registrar and ward workflows**

Implementation target:
- registrar pages for request intake/admit/discharge
- ward pages for bed map, current patient, transfer, discharge check
- conflict handling follows spec `keep_last_success`

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_inpatient_ward_workbench_flow`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/workbench/InpatientWorkbench.c src/ui/workbench/WardWorkbench.c tests/test_inpatient_ward_workbench_flow.c
git commit -m "feat: rebuild inpatient and ward desktop workflows"
```

### Task 10: Rebuild pharmacy and admin workbenches with downstream refresh behavior

**Files:**
- Modify: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `src/ui/workbench/AdminWorkbench.c`
- Modify: `tests/test_pharmacy_admin_workbench_flow.c`

- [ ] **Step 1: Write failing workflow tests for pharmacy/admin behavior**

Cover:
- pharmacy consumes doctor dispense requests and enforces stock conflicts
- pharmacy refreshes patient dispense history and doctor stock reference
- admin overview reflects cross-role changes
- admin maintenance entrypoints are available and role-restricted

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_pharmacy_admin_workbench_flow && ctest --test-dir build --output-on-failure -R test_pharmacy_admin_workbench_flow`
Expected: FAIL because pharmacy/admin pages are incomplete and refresh links are missing.

- [ ] **Step 3: Implement pharmacy and admin workbenches**

Implementation target:
- pharmacy stock/restock/dispense/low-stock/history pages
- admin overview and maintenance entry cards
- dirty propagation into admin summaries

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_pharmacy_admin_workbench_flow`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/workbench/PharmacyWorkbench.c src/ui/workbench/AdminWorkbench.c tests/test_pharmacy_admin_workbench_flow.c
git commit -m "feat: rebuild pharmacy and admin desktop workflows"
```

### Task 11: Seed demo-ready simulated data and integrity tests

**Files:**
- Modify: `data/users.txt`
- Modify: `data/patients.txt`
- Modify: `data/departments.txt`
- Modify: `data/doctors.txt`
- Modify: `data/registrations.txt`
- Modify: `data/visits.txt`
- Modify: `data/examinations.txt`
- Modify: `data/wards.txt`
- Modify: `data/beds.txt`
- Modify: `data/admissions.txt`
- Modify: `data/medicines.txt`
- Modify: `data/dispense_records.txt`
- Modify: `tests/test_demo_data_integrity.c`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

- [ ] **Step 1: Write failing demo data integrity tests**

Cover:
- at least 5 departments, 10 doctors, 12 beds, 15 medicines
- at least 3 ward types
- at least 8 core scenario patients
- `60-100` seeded patients overall
- historical registrations, visits, examinations, admissions, and dispense records all non-trivial
- low-stock medicines exist
- mixed occupied and available beds exist
- three complete cross-role chains exist in seeded data
- no occupied-bed or stock contradictions

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_demo_data_integrity && ctest --test-dir build --output-on-failure -R test_demo_data_integrity`
Expected: FAIL because current shipped data set is smaller and missing linked scenarios.

- [ ] **Step 3: Replace demo data with scenario-driven seed set**

Implementation target:
- keep existing role accounts
- add medium-size demo records
- ensure seeded registrations/visits/exams/admissions/dispenses line up with role workflows

- [ ] **Step 4: Document the new demo flows in README**

Update:
- role login list
- suggested demo sequence
- notes about self-service registration and cross-role storyline

- [ ] **Step 5: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_demo_data_integrity`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add data/users.txt data/patients.txt data/departments.txt data/doctors.txt data/registrations.txt data/visits.txt data/examinations.txt data/wards.txt data/beds.txt data/admissions.txt data/medicines.txt data/dispense_records.txt tests/test_demo_data_integrity.c CMakeLists.txt README.md
git commit -m "feat: add integrated desktop demo dataset"
```

## Chunk 5: Docs, Docker, GitHub Delivery, And Final Verification

### Task 12: Add complete install, build, run, and AI usage guides

**Files:**
- Modify: `README.md`
- Create: `docs/INSTALL.md`
- Create: `docs/BUILD_AND_RUN.md`
- Create: `docs/AI_USAGE.md`

- [ ] **Step 1: Write failing documentation checklist**

Create a checklist covering:
- human dependency installation on Windows/MSYS2
- direct local build and test commands
- Docker build and run commands
- portable package generation and usage
- AI-oriented repository usage guidance

- [ ] **Step 2: Draft human install guide**

Implementation target for `docs/INSTALL.md`:
- prerequisites
- MSYS2 / MinGW installation
- optional Git / CMake / Ninja notes
- environment variables and PATH setup
- common installation failures and fixes

- [ ] **Step 3: Draft build and run guide**

Implementation target for `docs/BUILD_AND_RUN.md`:
- direct build path
- direct test path
- smoke run and full desktop run
- Docker build and run path
- portable package generation path

- [ ] **Step 4: Draft AI usage guide**

Implementation target for `docs/AI_USAGE.md`:
- project structure summary
- recommended entry files
- build/test commands for AI agents
- dataset/regression caveats
- role workflow verification checklist for AI contributors

- [ ] **Step 5: Refresh README entrypoints**

Update README so it links clearly to:
- install guide
- build/run guide
- AI guide
- release/package instructions

- [ ] **Step 6: Commit**

```bash
git add README.md docs/INSTALL.md docs/BUILD_AND_RUN.md docs/AI_USAGE.md
git commit -m "docs: add install build and ai usage guides"
```

### Task 13: Add Docker build assets and GitHub portable release workflow

**Files:**
- Create: `Dockerfile`
- Create: `.dockerignore`
- Create: `.github/workflows/portable-release.yml`
- Modify: `README.md`

- [ ] **Step 1: Write failing delivery checklist**

Cover:
- Docker image can build the project
- Docker container can run build + tests
- GitHub Actions workflow can build the portable package on push/tag/manual dispatch
- workflow uploads the portable artifact

- [ ] **Step 2: Create Docker build assets**

Implementation target:
- `Dockerfile` supports non-interactive build/test packaging environment
- `.dockerignore` keeps build context small
- document any Windows-only limitations for actually running GUI binaries inside Docker

- [ ] **Step 3: Create GitHub Actions release workflow**

Implementation target for `.github/workflows/portable-release.yml`:
- trigger on workflow dispatch and release/tag path
- build project
- run tests
- run `scripts/package-release.ps1`
- upload portable zip as artifact
- if release context is available, attach the portable zip to the GitHub release

- [ ] **Step 4: Document Docker and release usage in README/build guide**

Update docs so a human can:
- build directly
- build in Docker
- download or generate the portable package

- [ ] **Step 5: Commit**

```bash
git add Dockerfile .dockerignore .github/workflows/portable-release.yml README.md docs/BUILD_AND_RUN.md
git commit -m "build: add docker and github portable release workflow"
```

### Task 14: Run full desktop, docs, packaging, and delivery verification

**Files:**
- Inspect only unless failures require fixes

- [ ] **Step 1: Build all desktop-related targets**

Run: `cmake -S . -B build && cmake --build build --target his_desktop test_menu_application test_desktop_state test_desktop_adapters test_desktop_workflows test_desktop_layout_rules test_demo_data_integrity test_patient_workbench_flow test_clerk_workbench_flow test_doctor_workbench_flow test_inpatient_ward_workbench_flow test_pharmacy_admin_workbench_flow`
Expected: PASS

- [ ] **Step 2: Run focused integrated desktop suite**

Run: `ctest --test-dir build --output-on-failure -R "test_menu_application|test_desktop_state|test_desktop_adapters|test_desktop_workflows|test_desktop_layout_rules|test_demo_data_integrity|test_patient_workbench_flow|test_clerk_workbench_flow|test_doctor_workbench_flow|test_inpatient_ward_workbench_flow|test_pharmacy_admin_workbench_flow"`
Expected: PASS

- [ ] **Step 3: Run full project suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 4: Run desktop smoke**

Run: `.\\build\\his_desktop.exe --smoke`
Expected: PASS

- [ ] **Step 5: Run manual desktop acceptance at both target resolutions**

Run:
- `.\\build\\his_desktop.exe`
- verify once at `1600x960`
- resize and verify again at `1366x768`

Expected:
- all seven roles land on the correct default home page
- no button-on-button overlap
- no button-on-input overlap
- no horizontal crop blocking primary actions
- Chinese renders legibly on role home pages and workflow pages

- [ ] **Step 6: Merge lane workflow assertions into integrator workflow suite**

Modify: `tests/test_desktop_workflows.c`

Implementation target:
- collect the strongest role-to-role assertions from lane-owned workflow tests
- keep `tests/test_desktop_workflows.c` as the final cross-role integration suite only
- explicitly assert all seven role default home pages and at least one role-switch/logout state reset path
- explicitly assert key freshness transitions used by the three cross-role chains

- [ ] **Step 7: Re-run full project suite after integrator merge**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 8: Run Docker validation**

Run:
- `docker build -t lightweight-his:local .`
- `docker run --rm lightweight-his:local`

Expected:
- image builds successfully
- container completes documented build/test command successfully

- [ ] **Step 9: Run packaging validation**

Run: `powershell -ExecutionPolicy Bypass -File .\\scripts\\package-release.ps1`
Expected: PASS and updated package assets appear under `dist/`

- [ ] **Step 10: Push source to GitHub**

Run:
- `git push origin HEAD`

Expected:
- latest source, docs, Docker assets, workflow, and release scripts are available on GitHub

- [ ] **Step 11: Publish portable version to GitHub**

Preferred path:
- trigger `.github/workflows/portable-release.yml` on GitHub or push an annotated tag used by the workflow

Fallback path:
- if GitHub release upload automation is unavailable, push the workflow and verify artifact upload path on the next run

Expected:
- portable build is available on GitHub as a workflow artifact or release asset

- [ ] **Step 12: Final commit**

```bash
git add .
git commit -m "feat: ship specialized integrated desktop his workbenches"
```

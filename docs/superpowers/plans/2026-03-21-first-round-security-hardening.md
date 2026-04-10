# First-Round Security Hardening Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the highest-risk findings in the HIS console app by adding minimal role/session enforcement for patient flows and hardening patient file parsing against overflow, with regression tests.

**Architecture:** Keep the current console/menu structure, but add a lightweight runtime session context inside `MenuApplication` so patient-facing actions are constrained to the bound patient identifier. Expose the smallest possible bind/reset API so direct `MenuApplication_execute_action` tests can drive the session without going through `main`. Harden `PatientRepository` parsing with bounded copies and explicit overlength rejection instead of broad refactors.

**Tech Stack:** C11, CMake, CTest, flat-file repositories, existing unit/integration tests in `tests/`

---

## Chunk 1: Patient Session And Authorization

### Task 1: Add failing authorization regression tests

**Files:**
- Modify: `tests/test_menu_application.c`
- Inspect: `include/ui/MenuApplication.h`
- Inspect: `src/ui/MenuApplication.c`
- Inspect: `src/main.c`

- [ ] **Step 1: Add regression tests for patient self-only access**

Add tests that prove:
- patient route cannot query another patient's basic info
- patient route cannot query another patient's registrations/history
- patient route cannot use dispense lookup as an IDOR bypass
- patient route still works for its own patient id

- [ ] **Step 2: Run the focused test target and verify it fails**

Run: `ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: FAIL in the new patient-authorization assertions

- [ ] **Step 3: Implement minimal session enforcement**

Modify:
- `include/ui/MenuApplication.h`
- `src/ui/MenuApplication.c`
- `src/main.c`

Implementation target:
- add current role and current patient id to `MenuApplication`
- add a minimal public bind/reset path so both `main` and tests can bind patient identity
- reject patient actions when requested `patient_id` does not match the bound session patient id
- fail closed for patient-only dispense lookup unless it can be safely scoped to the bound patient
- keep non-patient roles working as before

- [ ] **Step 4: Run focused tests to verify the authorization fix**

Run: `ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: PASS

### Task 2: Preserve existing patient menu behavior for own records

**Files:**
- Modify: `tests/test_menu_application.c`
- Inspect: `src/ui/MenuApplication.c`

- [ ] **Step 1: Add/adjust regression coverage for self access success**

Cover:
- patient basic info
- patient registration query
- patient history query
- denial path for patient dispense lookup when caller cannot prove ownership

- [ ] **Step 2: Run the focused test target and verify red/green**

Run: `ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: PASS after implementation if Task 1 already drove the required red/green cycle

## Chunk 2: Safe Patient Repository Parsing

### Task 3: Add failing overflow/oversize parsing tests

**Files:**
- Modify: `tests/test_patient_repository.c`
- Inspect: `src/repository/PatientRepository.c`
- Inspect: `include/domain/Patient.h`

- [ ] **Step 1: Add regression tests for oversized serialized patient fields**

Add tests that prove:
- a raw persisted line with an oversized `patient_id` is rejected during load/find
- a raw persisted line with an oversized `medical_history` is rejected during load/find
- repository does not silently truncate malformed persisted data

- [ ] **Step 2: Run the focused test target and verify it fails**

Run: `ctest --test-dir build --output-on-failure -R test_patient_repository`
Expected: FAIL in the new oversized-line assertions

- [ ] **Step 3: Implement bounded parse helpers**

Modify:
- `src/repository/PatientRepository.c`

Implementation target:
- replace direct `strcpy` field imports with bounded copy validation
- return a clear failure when any serialized field exceeds the destination capacity
- keep file format unchanged
- keep malformed fixture lines below `TEXT_FILE_REPOSITORY_LINE_CAPACITY` so the regression targets `PatientRepository_parse_line`, not the shared line reader

- [ ] **Step 4: Run the focused test target and verify it passes**

Run: `ctest --test-dir build --output-on-failure -R test_patient_repository`
Expected: PASS

## Chunk 3: Integration Verification

### Task 4: Re-run combined checks and inspect for regressions

**Files:**
- Inspect only: `CMakeLists.txt`
- Inspect only: modified production/test files from chunks 1-2

- [ ] **Step 1: Run the smallest combined regression suite**

Run: `ctest --test-dir build --output-on-failure -R "test_menu_application|test_patient_repository"`
Expected: PASS

- [ ] **Step 2: Run the full suite for integrated verification**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 3: Summarize remaining deferred findings**

Record but do not fix in this round:
- full authentication/account system
- multi-file transactional persistence rollback
- concurrent ID generation / file locking
- `MenuApplication` structural decomposition

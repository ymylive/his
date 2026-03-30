# Streaming Summary Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add reusable streaming repository scan interfaces plus a read-only summary service so dashboard statistics and recent records no longer depend on full `load_all()` materialization.

**Architecture:** Keep the existing text-repository format and parsing logic in repository files. Add `for_each(...)` read APIs to the five repositories needed by dashboard summaries, then build a `SummaryService` that computes counts and recent-5 windows in one pass per file, and finally rewire `DesktopAdapters_load_dashboard()` to consume that service while preserving current UI behavior.

**Tech Stack:** C11, CMake, existing repository/service layer, `TextFileRepository_for_each_data_line`, `LinkedList`, CTest

---

## File Structure

### Repository scan contracts

- Modify: `include/repository/PatientRepository.h`
- Modify: `src/repository/PatientRepository.c`
- Modify: `include/repository/RegistrationRepository.h`
- Modify: `src/repository/RegistrationRepository.c`
- Modify: `include/repository/AdmissionRepository.h`
- Modify: `src/repository/AdmissionRepository.c`
- Modify: `include/repository/MedicineRepository.h`
- Modify: `src/repository/MedicineRepository.c`
- Modify: `include/repository/DispenseRecordRepository.h`
- Modify: `src/repository/DispenseRecordRepository.c`

### Summary layer

- Create: `include/service/SummaryService.h`
- Create: `src/service/SummaryService.c`

### Desktop integration

- Modify: `src/ui/DesktopAdapters.c`

### Tests

- Create: `tests/test_repository_streaming.c`
- Create: `tests/test_summary_service.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `CMakeLists.txt`

### Reference docs

- Inspect: `docs/superpowers/specs/2026-03-29-streaming-summary-layer-design.md`
- Inspect: `src/ui/DesktopAdapters.c`

## Parallel Execution Ownership

Use separate workers only on disjoint files.

### Lane A: Patient / registration / admission scan APIs

- Owns:
  - `include/repository/PatientRepository.h`
  - `src/repository/PatientRepository.c`
  - `include/repository/RegistrationRepository.h`
  - `src/repository/RegistrationRepository.c`
  - `include/repository/AdmissionRepository.h`
  - `src/repository/AdmissionRepository.c`

### Lane B: Medicine / dispense scan APIs

- Owns:
  - `include/repository/MedicineRepository.h`
  - `src/repository/MedicineRepository.c`
  - `include/repository/DispenseRecordRepository.h`
  - `src/repository/DispenseRecordRepository.c`

### Integrator lane

- Owns:
  - `include/service/SummaryService.h`
  - `src/service/SummaryService.c`
  - `src/ui/DesktopAdapters.c`
  - `tests/test_repository_streaming.c`
  - `tests/test_summary_service.c`
  - `tests/test_desktop_adapters.c`
  - `CMakeLists.txt`

### Shared-file sequencing rules

- `tests/test_repository_streaming.c` and `CMakeLists.txt` are integrator-only to avoid merge conflicts.
- Lanes A and B land repository APIs first.
- `SummaryService` work starts only after all five `for_each(...)` interfaces compile.
- `DesktopAdapters.c` is touched only after `SummaryService` tests are green.

## Task 1: Add Patient / Registration / Admission Streaming Scan APIs

**Files:**
- Modify: `include/repository/PatientRepository.h`
- Modify: `src/repository/PatientRepository.c`
- Modify: `include/repository/RegistrationRepository.h`
- Modify: `src/repository/RegistrationRepository.c`
- Modify: `include/repository/AdmissionRepository.h`
- Modify: `src/repository/AdmissionRepository.c`
- Test: `tests/test_repository_streaming.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing repository streaming tests**

Add focused tests to `tests/test_repository_streaming.c` that lock the three behaviors needed by the summary layer, and register the new test target in `CMakeLists.txt`:

```c
typedef struct CountContext {
    int count;
} CountContext;

static Result count_patient(const Patient *patient, void *context) {
    CountContext *count = (CountContext *)context;
    assert(patient != 0);
    count->count++;
    return Result_make_success("continue");
}

static Result stop_after_first_registration(const Registration *registration, void *context) {
    CountContext *count = (CountContext *)context;
    assert(registration != 0);
    count->count++;
    return Result_make_failure("stop here");
}

static void test_patient_repository_for_each_counts_all_rows(void) {
    PatientRepository repository;
    CountContext context = {0};
    Result result = PatientRepository_for_each(&repository, count_patient, &context);
    assert(result.success == 1);
    assert(context.count == 2);
}

static void test_registration_repository_for_each_stops_on_visitor_failure(void) {
    RegistrationRepository repository;
    CountContext context = {0};
    Result result = RegistrationRepository_for_each(
        &repository,
        stop_after_first_registration,
        &context
    );
    assert(result.success == 0);
    assert(strcmp(result.message, "stop here") == 0);
    assert(context.count == 1);
}

static void test_admission_repository_for_each_rejects_invalid_line(void) {
    AdmissionRepository repository;
    CountContext context = {0};
    Result result = AdmissionRepository_for_each(&repository, count_admission, &context);
    assert(result.success == 0);
    assert(strstr(result.message, "invalid") != 0 || strstr(result.message, "too long") != 0);
}
```

```cmake
add_executable(test_repository_streaming
    tests/test_repository_streaming.c
)

target_link_libraries(test_repository_streaming PRIVATE his_common)

add_test(NAME test_repository_streaming COMMAND test_repository_streaming)
```

- [ ] **Step 2: Run the focused repository test to verify red**

Run:

```bash
cmake -S . -B build
cmake --build build --target test_repository_streaming
ctest --test-dir build --output-on-failure -R test_repository_streaming
```

Expected: FAIL because `PatientRepository_for_each`, `RegistrationRepository_for_each`, and `AdmissionRepository_for_each` do not exist yet.

- [ ] **Step 3: Add the three streaming contracts and minimal implementations**

Add visitor typedefs to the headers:

```c
typedef Result (*PatientRepositoryVisitor)(const Patient *patient, void *context);
Result PatientRepository_for_each(
    const PatientRepository *repository,
    PatientRepositoryVisitor visitor,
    void *context
);
```

```c
typedef Result (*RegistrationRepositoryVisitor)(const Registration *registration, void *context);
Result RegistrationRepository_for_each(
    const RegistrationRepository *repository,
    RegistrationRepositoryVisitor visitor,
    void *context
);
```

```c
typedef Result (*AdmissionRepositoryVisitor)(const Admission *admission, void *context);
Result AdmissionRepository_for_each(
    const AdmissionRepository *repository,
    AdmissionRepositoryVisitor visitor,
    void *context
);
```

Implement them in the repository sources by reusing the existing parse helpers instead of duplicating field parsing:

```c
typedef struct PatientRepositoryForEachContext {
    PatientRepositoryVisitor visitor;
    void *context;
} PatientRepositoryForEachContext;

static Result PatientRepository_for_each_line_handler(const char *line, void *context) {
    PatientRepositoryForEachContext *for_each_context = (PatientRepositoryForEachContext *)context;
    Patient patient;
    Result result = PatientRepository_parse_line(line, &patient);

    if (result.success == 0) {
        return result;
    }

    return for_each_context->visitor(&patient, for_each_context->context);
}

Result PatientRepository_for_each(
    const PatientRepository *repository,
    PatientRepositoryVisitor visitor,
    void *context
) {
    PatientRepositoryForEachContext for_each_context;

    if (repository == 0 || visitor == 0) {
        return Result_make_failure("patient repository visitor missing");
    }

    for_each_context.visitor = visitor;
    for_each_context.context = context;
    return TextFileRepository_for_each_data_line(
        &repository->file_repository,
        PatientRepository_for_each_line_handler,
        &for_each_context
    );
}
```

Mirror that pattern in the registration and admission repositories using `RegistrationRepository_parse_line` and `AdmissionRepository_parse_line`.

- [ ] **Step 4: Re-run the repository streaming test**

Run:

```bash
cmake --build build --target test_repository_streaming
ctest --test-dir build --output-on-failure -R test_repository_streaming
```

Expected: PASS for the patient/registration/admission cases while the medicine and dispense cases are still absent or skipped.

- [ ] **Step 5: Commit the first repository lane**

```bash
git add include/repository/PatientRepository.h src/repository/PatientRepository.c include/repository/RegistrationRepository.h src/repository/RegistrationRepository.c include/repository/AdmissionRepository.h src/repository/AdmissionRepository.c tests/test_repository_streaming.c CMakeLists.txt
git commit -m "feat: add streaming scans for patient registration and admission"
```

## Task 2: Add Medicine / Dispense Streaming Scan APIs

**Files:**
- Modify: `include/repository/MedicineRepository.h`
- Modify: `src/repository/MedicineRepository.c`
- Modify: `include/repository/DispenseRecordRepository.h`
- Modify: `src/repository/DispenseRecordRepository.c`
- Test: `tests/test_repository_streaming.c`

- [ ] **Step 1: Extend the failing repository streaming tests for inventory data**

Append medicine and dispense coverage to `tests/test_repository_streaming.c`:

```c
static Result count_medicine(const Medicine *medicine, void *context) {
    CountContext *count = (CountContext *)context;
    assert(medicine != 0);
    count->count++;
    return Result_make_success("continue");
}

static Result count_dispense(const DispenseRecord *record, void *context) {
    CountContext *count = (CountContext *)context;
    assert(record != 0);
    count->count++;
    return Result_make_success("continue");
}

static void test_medicine_repository_for_each_counts_all_rows(void) {
    MedicineRepository repository;
    CountContext context = {0};
    Result result = MedicineRepository_for_each(&repository, count_medicine, &context);
    assert(result.success == 1);
    assert(context.count == 3);
}

static void test_dispense_repository_for_each_counts_all_rows(void) {
    DispenseRecordRepository repository;
    CountContext context = {0};
    Result result = DispenseRecordRepository_for_each(&repository, count_dispense, &context);
    assert(result.success == 1);
    assert(context.count == 2);
}
```

- [ ] **Step 2: Run the focused repository test to verify red**

Run:

```bash
cmake --build build --target test_repository_streaming
ctest --test-dir build --output-on-failure -R test_repository_streaming
```

Expected: FAIL because `MedicineRepository_for_each` and `DispenseRecordRepository_for_each` do not exist yet.

- [ ] **Step 3: Add the remaining two streaming contracts and implementations**

Add the header contracts:

```c
typedef Result (*MedicineRepositoryVisitor)(const Medicine *medicine, void *context);
Result MedicineRepository_for_each(
    MedicineRepository *repository,
    MedicineRepositoryVisitor visitor,
    void *context
);
```

```c
typedef Result (*DispenseRecordRepositoryVisitor)(const DispenseRecord *record, void *context);
Result DispenseRecordRepository_for_each(
    const DispenseRecordRepository *repository,
    DispenseRecordRepositoryVisitor visitor,
    void *context
);
```

Implement them by reusing the existing parse helpers:

```c
static Result MedicineRepository_for_each_line_handler(const char *line, void *context) {
    MedicineRepositoryForEachContext *for_each_context = (MedicineRepositoryForEachContext *)context;
    Medicine medicine;
    Result result = MedicineRepository_parse_line(line, &medicine);

    if (result.success == 0) {
        return result;
    }

    return for_each_context->visitor(&medicine, for_each_context->context);
}
```

```c
static Result DispenseRecordRepository_for_each_line_handler(const char *line, void *context) {
    DispenseRecordRepositoryForEachContext *for_each_context =
        (DispenseRecordRepositoryForEachContext *)context;
    DispenseRecord record;
    Result result = DispenseRecordRepository_parse_line(line, &record);

    if (result.success == 0) {
        return result;
    }

    return for_each_context->visitor(&record, for_each_context->context);
}
```

- [ ] **Step 4: Re-run the repository streaming test**

Run:

```bash
cmake --build build --target test_repository_streaming
ctest --test-dir build --output-on-failure -R test_repository_streaming
```

Expected: PASS with all five repository streaming cases green.

- [ ] **Step 5: Commit the second repository lane**

```bash
git add include/repository/MedicineRepository.h src/repository/MedicineRepository.c include/repository/DispenseRecordRepository.h src/repository/DispenseRecordRepository.c tests/test_repository_streaming.c
git commit -m "feat: add streaming scans for medicine and dispense records"
```

## Task 3: Build SummaryService And Rewire Dashboard Loading

**Files:**
- Create: `include/service/SummaryService.h`
- Create: `src/service/SummaryService.c`
- Modify: `src/ui/DesktopAdapters.c`
- Create: `tests/test_summary_service.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing summary and dashboard regression tests**

Create `tests/test_summary_service.c` with a snapshot type and recent-window assertions, and register the target in `CMakeLists.txt`:

```c
static void test_summary_service_load_dashboard_counts_and_recent_windows(void) {
    SummaryService service;
    SummaryDashboardData dashboard;
    Result result;

    SummaryDashboardData_init(&dashboard);
    result = SummaryService_init(
        &service,
        &patient_repository,
        &registration_repository,
        &admission_repository,
        &medicine_repository,
        &dispense_repository
    );
    assert(result.success == 1);

    result = SummaryService_load_dashboard(&service, &dashboard);
    assert(result.success == 1);
    assert(dashboard.patient_count == 2);
    assert(dashboard.registration_count == 6);
    assert(dashboard.inpatient_count == 1);
    assert(dashboard.low_stock_count == 1);
    assert(LinkedList_count(&dashboard.recent_registrations) == 5);
    assert(LinkedList_count(&dashboard.recent_dispensations) == 5);
}
```

```cmake
add_executable(test_summary_service
    tests/test_summary_service.c
)

target_link_libraries(test_summary_service PRIVATE his_common)

add_test(NAME test_summary_service COMMAND test_summary_service)
```

Tighten `tests/test_desktop_adapters.c` so the adapter integration locks exact behavior instead of loose `>= 1` checks:

```c
assert(dashboard.patient_count == 1);
assert(dashboard.registration_count == 6);
assert(dashboard.inpatient_count == 1);
assert(dashboard.low_stock_count == 1);
assert(strcmp(((Registration *)dashboard.recent_registrations.head->data)->registered_at, "2026-03-21T09:05") == 0);
assert(strcmp(((DispenseRecord *)dashboard.recent_dispensations.head->data)->dispensed_at, "2026-03-21T10:05") == 0);
```

- [ ] **Step 2: Run focused verification and confirm red**

Run:

```bash
cmake --build build --target test_summary_service test_desktop_adapters
ctest --test-dir build --output-on-failure -R "test_summary_service|test_desktop_adapters"
```

Expected: FAIL because `SummaryService` does not exist yet and the stricter adapter assertions are not implemented.

- [ ] **Step 3: Implement the summary layer with fixed-size recent windows**

Define the public summary types in `include/service/SummaryService.h` without pulling in desktop UI headers:

```c
typedef struct SummaryDashboardData {
    int patient_count;
    int registration_count;
    int inpatient_count;
    int low_stock_count;
    LinkedList recent_registrations;
    LinkedList recent_dispensations;
} SummaryDashboardData;

typedef struct SummaryService {
    const PatientRepository *patient_repository;
    const RegistrationRepository *registration_repository;
    const AdmissionRepository *admission_repository;
    MedicineRepository *medicine_repository;
    const DispenseRecordRepository *dispense_repository;
} SummaryService;

void SummaryDashboardData_init(SummaryDashboardData *dashboard);
void SummaryDashboardData_clear(SummaryDashboardData *dashboard);
Result SummaryService_init(
    SummaryService *service,
    const PatientRepository *patient_repository,
    const RegistrationRepository *registration_repository,
    const AdmissionRepository *admission_repository,
    MedicineRepository *medicine_repository,
    const DispenseRecordRepository *dispense_repository
);
Result SummaryService_load_dashboard(SummaryService *service, SummaryDashboardData *out_dashboard);
```

Add the new service source to `his_common` in `CMakeLists.txt`:

```cmake
add_library(his_common
    src/common/IdGenerator.c
    src/common/InputHelper.c
    src/common/LinkedList.c
    src/common/Result.c
    src/common/StringUtils.c
    src/service/SummaryService.c
    ...
)
```

Implement one-pass aggregation in `src/service/SummaryService.c`:

```c
typedef struct SummaryRecentRegistrationWindow {
    Registration items[5];
    size_t count;
} SummaryRecentRegistrationWindow;

static void SummaryRecentRegistrationWindow_push(
    SummaryRecentRegistrationWindow *window,
    const Registration *registration
) {
    size_t index = 0;

    if (window->count < 5) {
        window->items[window->count++] = *registration;
        return;
    }

    for (index = 1; index < 5; index++) {
        window->items[index - 1] = window->items[index];
    }
    window->items[4] = *registration;
}
```

Use the repository `for_each(...)` APIs to update counts and windows, then materialize the two recent windows into linked lists in newest-first order.

Finally, rewire `DesktopAdapters_load_dashboard()` to use the service and transfer list ownership:

```c
SummaryService summary_service;
SummaryDashboardData summary;

result = SummaryService_init(
    &summary_service,
    &application->patient_service.repository,
    &application->registration_repository,
    &application->inpatient_service.admission_repository,
    &application->pharmacy_service.medicine_repository,
    &application->pharmacy_service.dispense_record_repository
);
if (result.success == 0) {
    return result;
}

SummaryDashboardData_init(&summary);
result = SummaryService_load_dashboard(&summary_service, &summary);
if (result.success == 0) {
    SummaryDashboardData_clear(&summary);
    return result;
}

dashboard->patient_count = summary.patient_count;
dashboard->registration_count = summary.registration_count;
dashboard->inpatient_count = summary.inpatient_count;
dashboard->low_stock_count = summary.low_stock_count;
dashboard->recent_registrations = summary.recent_registrations;
dashboard->recent_dispensations = summary.recent_dispensations;
LinkedList_init(&summary.recent_registrations);
LinkedList_init(&summary.recent_dispensations);
dashboard->loaded = 1;
SummaryDashboardData_clear(&summary);
```

- [ ] **Step 4: Re-run focused verification**

Run:

```bash
cmake --build build --target test_repository_streaming test_summary_service test_desktop_adapters
ctest --test-dir build --output-on-failure -R "test_repository_streaming|test_summary_service|test_desktop_adapters"
```

Expected: PASS. `test_summary_service` proves the reusable summary layer, and `test_desktop_adapters` proves dashboard behavior stayed stable.

- [ ] **Step 5: Commit the summary-layer integration**

```bash
git add include/service/SummaryService.h src/service/SummaryService.c src/ui/DesktopAdapters.c tests/test_summary_service.c tests/test_desktop_adapters.c CMakeLists.txt
git commit -m "feat: add streaming summary service for dashboard"
```

## Task 4: Full Verification And Handoff

**Files:**
- Inspect only

- [ ] **Step 1: Run the full relevant test slice**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R "test_repository_streaming|test_summary_service|test_desktop_adapters|test_desktop_workflows"
```

Expected: PASS

- [ ] **Step 2: Run a diff review focused on the summary layer**

Run:

```bash
git diff --stat HEAD~3..HEAD
git diff -- include/repository/PatientRepository.h include/repository/RegistrationRepository.h include/repository/AdmissionRepository.h include/repository/MedicineRepository.h include/repository/DispenseRecordRepository.h include/service/SummaryService.h src/service/SummaryService.c src/ui/DesktopAdapters.c tests/test_repository_streaming.c tests/test_summary_service.c tests/test_desktop_adapters.c CMakeLists.txt
```

Expected: Only the planned files changed, and there is no accidental write-path behavior change.

- [ ] **Step 3: Final commit if verification required follow-up edits**

If verification changed files:

```bash
git add include/repository/PatientRepository.h src/repository/PatientRepository.c include/repository/RegistrationRepository.h src/repository/RegistrationRepository.c include/repository/AdmissionRepository.h src/repository/AdmissionRepository.c include/repository/MedicineRepository.h src/repository/MedicineRepository.c include/repository/DispenseRecordRepository.h src/repository/DispenseRecordRepository.c include/service/SummaryService.h src/service/SummaryService.c src/ui/DesktopAdapters.c tests/test_repository_streaming.c tests/test_summary_service.c tests/test_desktop_adapters.c CMakeLists.txt
git commit -m "test: finalize streaming summary layer verification"
```

- [ ] **Step 4: Prepare handoff notes**

Capture:

```text
- repositories now expose streaming read APIs for summary use
- dashboard loading no longer depends on five full load_all() calls
- SummaryService is ready for future time-range or recent-record queries
```

## Self-Review Checklist

- Spec coverage:
  - repository streaming interfaces: Task 1 and Task 2
  - reusable summary layer: Task 3
  - dashboard integration: Task 3
  - no caching / always current: Task 3 implementation path only reads live repositories
  - tests and regression safety: Task 1, Task 2, Task 3, Task 4
- Placeholder scan:
  - no placeholder markers remain
- Type consistency:
  - repository visitor names follow `XxxRepositoryVisitor`
  - summary result type is `SummaryDashboardData`
  - adapter integration consumes `SummaryService_load_dashboard`

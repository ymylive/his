#ifndef HIS_UI_DESKTOP_APP_H
#define HIS_UI_DESKTOP_APP_H

#include "common/LinkedList.h"
#include "domain/DispenseRecord.h"
#include "domain/Patient.h"
#include "domain/Registration.h"
#include "domain/User.h"
#include "ui/DesktopTheme.h"
#include "ui/MenuApplication.h"

typedef enum DesktopPage {
    DESKTOP_PAGE_LOGIN = 0,
    DESKTOP_PAGE_DASHBOARD = 1,
    DESKTOP_PAGE_PATIENTS = 2,
    DESKTOP_PAGE_REGISTRATION = 3,
    DESKTOP_PAGE_DISPENSE = 4,
    DESKTOP_PAGE_SYSTEM = 5,
    DESKTOP_PAGE_DOCTOR = 6,
    DESKTOP_PAGE_INPATIENT = 7,
    DESKTOP_PAGE_PHARMACY = 8
} DesktopPage;

typedef enum DesktopMessageKind {
    DESKTOP_MESSAGE_NONE = 0,
    DESKTOP_MESSAGE_INFO = 1,
    DESKTOP_MESSAGE_SUCCESS = 2,
    DESKTOP_MESSAGE_WARNING = 3,
    DESKTOP_MESSAGE_ERROR = 4
} DesktopMessageKind;

typedef enum DesktopPatientSearchMode {
    DESKTOP_PATIENT_SEARCH_BY_ID = 0,
    DESKTOP_PATIENT_SEARCH_BY_NAME = 1
} DesktopPatientSearchMode;

typedef struct DesktopMessage {
    DesktopMessageKind kind;
    char text[RESULT_MESSAGE_CAPACITY];
    int visible;
} DesktopMessage;

typedef struct DesktopLoginForm {
    char user_id[HIS_DOMAIN_ID_CAPACITY];
    char password[64];
    int role_index;
    int active_field;
    int password_edit_mode;
} DesktopLoginForm;

typedef struct DesktopDashboardState {
    int patient_count;
    int registration_count;
    int inpatient_count;
    int low_stock_count;
    LinkedList recent_registrations;
    LinkedList recent_dispensations;
    int loaded;
} DesktopDashboardState;

typedef struct DesktopPatientPageState {
    char query[HIS_DOMAIN_TEXT_CAPACITY];
    DesktopPatientSearchMode search_mode;
    LinkedList results;
    int active_field;
    int selected_index;
    Patient selected_patient;
    int has_selected_patient;
} DesktopPatientPageState;

typedef struct DesktopRegistrationPageState {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char registered_at[HIS_DOMAIN_TIME_CAPACITY];
    int active_field;
    char last_registration_id[HIS_DOMAIN_ID_CAPACITY];
} DesktopRegistrationPageState;

typedef struct DesktopDispensePageState {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    LinkedList results;
    int active_field;
    int loaded;
} DesktopDispensePageState;

typedef struct DesktopDoctorPageState {
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char registration_id[HIS_DOMAIN_ID_CAPACITY];
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char examination_id[HIS_DOMAIN_ID_CAPACITY];
    char chief_complaint[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char diagnosis[HIS_DOMAIN_TEXT_CAPACITY];
    char advice[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char exam_item[HIS_DOMAIN_TEXT_CAPACITY];
    char exam_type[HIS_DOMAIN_TEXT_CAPACITY];
    char exam_result[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char visit_time[HIS_DOMAIN_TIME_CAPACITY];
    char requested_at[HIS_DOMAIN_TIME_CAPACITY];
    char completed_at[HIS_DOMAIN_TIME_CAPACITY];
    int active_field;
    int need_exam;
    int need_admission;
    int need_medicine;
    char output[8192];
} DesktopDoctorPageState;

typedef struct DesktopInpatientPageState {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char ward_id[HIS_DOMAIN_ID_CAPACITY];
    char bed_id[HIS_DOMAIN_ID_CAPACITY];
    char admission_id[HIS_DOMAIN_ID_CAPACITY];
    char query_patient_id[HIS_DOMAIN_ID_CAPACITY];
    char query_bed_id[HIS_DOMAIN_ID_CAPACITY];
    char admitted_at[HIS_DOMAIN_TIME_CAPACITY];
    char discharged_at[HIS_DOMAIN_TIME_CAPACITY];
    char summary[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    int active_field;
    char output[8192];
} DesktopInpatientPageState;

typedef struct DesktopPharmacyPageState {
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];
    char medicine_name[HIS_DOMAIN_NAME_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];
    char pharmacist_id[HIS_DOMAIN_ID_CAPACITY];
    char dispensed_at[HIS_DOMAIN_TIME_CAPACITY];
    char price_text[32];
    char stock_text[16];
    char low_stock_text[16];
    char restock_quantity_text[16];
    char dispense_quantity_text[16];
    char stock_query_medicine_id[HIS_DOMAIN_ID_CAPACITY];
    int active_field;
    char output[8192];
} DesktopPharmacyPageState;

typedef struct DesktopAppState {
    DesktopPage current_page;
    DesktopMessage message;
    DesktopLoginForm login_form;
    DesktopDashboardState dashboard;
    DesktopPatientPageState patient_page;
    DesktopRegistrationPageState registration_page;
    DesktopDispensePageState dispense_page;
    DesktopDoctorPageState doctor_page;
    DesktopInpatientPageState inpatient_page;
    DesktopPharmacyPageState pharmacy_page;
    User current_user;
    int logged_in;
    int should_close;
} DesktopAppState;

typedef struct DesktopApp {
    MenuApplication application;
    const MenuApplicationPaths *paths;
    DesktopTheme theme;
    DesktopAppState state;
} DesktopApp;

typedef struct DesktopAppConfig {
    const MenuApplicationPaths *paths;
    int width;
    int height;
    const char *title;
    int smoke_mode;
} DesktopAppConfig;

void DesktopAppState_init(DesktopAppState *state);
void DesktopAppState_dispose(DesktopAppState *state);
void DesktopAppState_login_success(DesktopAppState *state, const User *user);
void DesktopAppState_logout(DesktopAppState *state);
void DesktopAppState_set_page(DesktopAppState *state, DesktopPage page);
void DesktopAppState_show_message(
    DesktopAppState *state,
    DesktopMessageKind kind,
    const char *text
);
const char *DesktopApp_page_label(DesktopPage page);
const char *DesktopApp_user_role_label(UserRole role);
UserRole DesktopApp_role_from_index(int index);
int DesktopApp_role_index_from_role(UserRole role);
DesktopPage DesktopApp_home_page_for_role(UserRole role);
int DesktopApp_run(const DesktopAppConfig *config);

#endif

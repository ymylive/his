#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "common/InputHelper.h"
#include "common/LinkedList.h"

typedef struct TestPerson {
    char id[16];
    int age;
} TestPerson;

static int TestPerson_has_id(const void *data, const void *context) {
    const TestPerson *person = (const TestPerson *)data;
    const char *expected_id = (const char *)context;

    return strcmp(person->id, expected_id) == 0;
}

/* ── InputHelper_is_esc_cancel tests ──────────────────────── */

static void test_input_helper_is_esc_cancel_with_sentinel(void) {
    assert(InputHelper_is_esc_cancel(INPUT_HELPER_ESC_MESSAGE) == 1);
}

static void test_input_helper_is_esc_cancel_with_null(void) {
    assert(InputHelper_is_esc_cancel(0) == 0);
}

static void test_input_helper_is_esc_cancel_with_other_message(void) {
    assert(InputHelper_is_esc_cancel("some other error") == 0);
    assert(InputHelper_is_esc_cancel("") == 0);
    assert(InputHelper_is_esc_cancel("input cancelled") == 0);
}

/* ── InputHelper_read_line tests (pipe/non-tty) ───────────── */

static void test_input_helper_read_line_normal_input(void) {
    FILE *f = tmpfile();
    char buf[64];
    int rc;
    assert(f != 0);
    fputs("hello\n", f);
    rewind(f);
    rc = InputHelper_read_line(f, buf, sizeof(buf));
    assert(rc == 1);
    assert(strcmp(buf, "hello") == 0);
    fclose(f);
}

static void test_input_helper_read_line_eof(void) {
    FILE *f = tmpfile();
    char buf[64];
    int rc;
    assert(f != 0);
    /* empty file → EOF */
    rewind(f);
    rc = InputHelper_read_line(f, buf, sizeof(buf));
    assert(rc == 0);
    fclose(f);
}

static void test_input_helper_read_line_overflow(void) {
    FILE *f = tmpfile();
    char buf[8]; /* small buffer */
    int rc;
    assert(f != 0);
    fputs("this is a very long line that exceeds buffer\n", f);
    rewind(f);
    rc = InputHelper_read_line(f, buf, sizeof(buf));
    assert(rc == -1);
    assert(buf[0] == '\0');
    fclose(f);
}

static void test_input_helper_read_line_null_buffer(void) {
    assert(InputHelper_read_line(stdin, 0, 64) == 0);
}

static void test_input_helper_read_line_zero_capacity(void) {
    char buf[8];
    assert(InputHelper_read_line(stdin, buf, 0) == 0);
}

static void test_input_helper_read_line_null_input(void) {
    char buf[64];
    int rc = InputHelper_read_line(0, buf, sizeof(buf));
    assert(rc == 0);
    assert(buf[0] == '\0');
}

static void test_input_helper_read_line_strips_newline(void) {
    FILE *f = tmpfile();
    char buf[64];
    int rc;
    assert(f != 0);
    fputs("test\n", f);
    rewind(f);
    rc = InputHelper_read_line(f, buf, sizeof(buf));
    assert(rc == 1);
    assert(strcmp(buf, "test") == 0);
    assert(strchr(buf, '\n') == 0);
    fclose(f);
}

static void test_input_helper_read_line_empty_line(void) {
    FILE *f = tmpfile();
    char buf[64];
    int rc;
    assert(f != 0);
    fputs("\n", f);
    rewind(f);
    rc = InputHelper_read_line(f, buf, sizeof(buf));
    assert(rc == 1);
    assert(buf[0] == '\0');
    fclose(f);
}

int main(void) {
    char identifier[16];
    LinkedList list;
    TestPerson patient_a = {"PAT0001", 21};
    TestPerson patient_b = {"PAT0002", 35};
    TestPerson patient_c = {"PAT0003", 42};
    TestPerson *found = 0;

    IdGenerator_format(identifier, sizeof(identifier), "PAT", 1, 4);
    assert(strcmp(identifier, "PAT0001") == 0);

    IdGenerator_format(identifier, sizeof(identifier), "PAT", 42, 4);
    assert(strcmp(identifier, "PAT0042") == 0);

    assert(InputHelper_is_non_empty("Alice") == 1);
    assert(InputHelper_is_non_empty("   ") == 0);
    assert(InputHelper_is_positive_integer("18") == 1);
    assert(InputHelper_is_positive_integer("0") == 0);
    assert(InputHelper_is_non_negative_amount("0") == 1);
    assert(InputHelper_is_non_negative_amount("12.50") == 1);
    assert(InputHelper_is_non_negative_amount("-2.00") == 0);

    LinkedList_init(&list);
    assert(LinkedList_count(&list) == 0);
    assert(LinkedList_find_first(&list, TestPerson_has_id, "PAT0002") == 0);

    assert(LinkedList_append(&list, &patient_a) == 1);
    assert(LinkedList_append(&list, &patient_b) == 1);
    assert(LinkedList_append(&list, &patient_c) == 1);
    assert(LinkedList_count(&list) == 3);

    found = (TestPerson *)LinkedList_find_first(&list, TestPerson_has_id, "PAT0002");
    assert(found == &patient_b);

    assert(LinkedList_remove_first(&list, TestPerson_has_id, "PAT0001") == 1);
    assert(LinkedList_count(&list) == 2);

    assert(LinkedList_remove_first(&list, TestPerson_has_id, "PAT0003") == 1);
    assert(LinkedList_count(&list) == 1);

    found = (TestPerson *)LinkedList_find_first(&list, TestPerson_has_id, "PAT0002");
    assert(found == &patient_b);

    assert(LinkedList_remove_first(&list, TestPerson_has_id, "PAT0002") == 1);
    assert(LinkedList_count(&list) == 0);

    LinkedList_clear(&list, 0);

    /* InputHelper_is_esc_cancel */
    test_input_helper_is_esc_cancel_with_sentinel();
    test_input_helper_is_esc_cancel_with_null();
    test_input_helper_is_esc_cancel_with_other_message();

    /* InputHelper_read_line */
    test_input_helper_read_line_normal_input();
    test_input_helper_read_line_eof();
    test_input_helper_read_line_overflow();
    test_input_helper_read_line_null_buffer();
    test_input_helper_read_line_zero_capacity();
    test_input_helper_read_line_null_input();
    test_input_helper_read_line_strips_newline();
    test_input_helper_read_line_empty_line();

    return 0;
}

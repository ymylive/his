#include <assert.h>
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
    return 0;
}

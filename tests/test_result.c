#include <assert.h>
#include <string.h>

#include "common/Result.h"

int main(void) {
    Result ok = Result_make_success("ok");
    Result fail = Result_make_failure("fail");

    assert(ok.success == 1);
    assert(strcmp(ok.message, "ok") == 0);

    assert(fail.success == 0);
    assert(strcmp(fail.message, "fail") == 0);

    return 0;
}

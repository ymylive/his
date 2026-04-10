/**
 * @file regen_user_fixture.c
 * @brief Regenerate data/users.txt fixture with current hash parameters.
 *
 * Usage:  ./regen_user_fixture > ../data/users.txt
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct DemoAccount {
    const char *user_id;
    const char *password;
    const char *salt_hex;
    int role;
} DemoAccount;

int main(void) {
    DemoAccount accounts[] = {
        {"ADM0001", "admin123",      "0a1b2c3d4e5f60718293a4b5c6d7e8f9", 3},
        {"DOC0001", "doctor123",     "2c3d4e5f60718293a4b5c6d7e8f90a1b", 2},
        {"INP0001", "inpatient123",  "3d4e5f60718293a4b5c6d7e8f90a1b2c", 5},
        {"PHA0001", "pharmacy123",   "5f60718293a4b5c6d7e8f90a1b2c3d4e", 7},
        {"PAT0001", "patient123",    "60718293a4b5c6d7e8f90a1b2c3d4e5f", 1},
    };
    int count = sizeof(accounts) / sizeof(accounts[0]);
    int i;

    printf("user_id|password_hash|role\n");
    for (i = 0; i < count; i++) {
        uint64_t h1 = UINT64_C(1469598103934665603);
        uint64_t h2 = UINT64_C(1099511628211);
        uint64_t h3 = UINT64_C(7809847782465536322);
        uint64_t h4 = UINT64_C(9650029242287828579);
        const char *salt = accounts[i].salt_hex;
        const char *pw = accounts[i].password;
        size_t idx;
        int iter;

        for (idx = 0; idx < 32; idx++) {
            unsigned char v = (unsigned char)salt[idx];
            h1 ^= (uint64_t)(v + 7U);
            h1 *= UINT64_C(1099511628211);
            h2 ^= (uint64_t)(v + 13U);
            h2 *= UINT64_C(1469598103934665603);
        }
        for (iter = 0; iter < 100000; iter++) {
            idx = 0;
            while (pw[idx] != '\0') {
                unsigned char v = (unsigned char)pw[idx];
                h1 ^= (uint64_t)(v + 17U);
                h1 *= UINT64_C(1099511628211);
                h2 ^= (uint64_t)(v + 37U);
                h2 *= UINT64_C(1469598103934665603);
                h3 ^= (uint64_t)(v + 73U);
                h3 *= UINT64_C(1099511628211);
                h4 ^= (uint64_t)(v + 131U);
                h4 *= UINT64_C(1469598103934665603);
                idx++;
            }
            h1 ^= h4;
            h2 ^= h3;
            h3 ^= h1;
            h4 ^= h2;
        }
        printf("%s|%s$%016llx%016llx%016llx%016llx|%d\n",
            accounts[i].user_id, salt,
            (unsigned long long)h1, (unsigned long long)h2,
            (unsigned long long)h3, (unsigned long long)h4,
            accounts[i].role);
    }
    return 0;
}

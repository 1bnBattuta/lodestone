/**
 * @file test.h
 * @author Omar Merroun
 * @brief shared unit test utilities
 * @version 0.1
 * @date 2026-09-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef LS_TEST_H
#define LS_TEST_H
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } } while (0)

#define CHECK_EQ_INT(a, b) do { long _a = (a), _b = (b); \
    if (_a != _b) { \
        fprintf(stderr, "%s:%d: FAIL: %s == %s (%ld vs %ld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        failures++; \
    } } while (0)

#define CHECK_EQ_STR(a, b) CHECK(strcmp((a), (b)) == 0)

#define RUN(test) do { printf("  %s\n", #test); test(); } while (0)

#define TEST_RESULT() \
    (printf(test_failures ? "FAILED (%d)\n" : "OK\n", failures), \
     failures != 0)

#endif // LS_TEST_H
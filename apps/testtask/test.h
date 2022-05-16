
#ifdef  RUN_OS_TEST
#define TEST(x)     { int rc; printf("TEST(0x%x) running ...\n", (x)); rc = test(x); \
                      printf("TEST(0x%x) %s\n", (x), rc ? "failed" : "passed"); if (rc) break; }
#else
#define TEST(x)
#endif

int test(int);


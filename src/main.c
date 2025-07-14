// #include "luaaux.h"
#include "unity.c"

// \src\luaaux.c ..\src\luado.c ..\src\luamem.c ..\src\luastate.c

extern void p1_test_main();
extern void p2_test_main();
extern void p3_test_main();

int main(int argc, char **argv)
{
    // p1_test_main();
    // p2_test_main();
    p3_test_main();
    system("pause");
    return 0;
}
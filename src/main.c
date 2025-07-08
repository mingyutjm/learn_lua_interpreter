#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

static jmp_buf b;

#define TRY if (setjmp(b) == 0)
#define CATCH else
#define THROW longjmp(b, 1)

#define LUAI_TRY(L, c, a)    \
    if (setjmp((c)->b) == 0) \
    {                        \
        a                    \
    }
#define LUAI_THROW(L, c) longjmp((c)->b, 1)
#define luai_jmpbuf jmp_buf

void foo()
{
    printf("foo\n");
    THROW;
}

int main()
{
    TRY
    {
        foo();
    }
    CATCH
    {
        printf("catch\n");
    }

    return 0;
}
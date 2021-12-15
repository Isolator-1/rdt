#include <stdio.h>
#define A
#define B

int main()
{
#ifdef A
#ifdef B
    printf("A B\n");
#else
    printf("A\n");
#endif
#else
#ifdef B
    printf("B\n");
#else
    printf("\n");
#endif
#endif
}
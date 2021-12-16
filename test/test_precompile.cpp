/**
 * @file test_precompile.cpp
 * @author zqs
 * @brief 测试预编译
 * @version 0.1
 * @date 2021-12-16
 * 
 * @copyright Copyright (c) 2021
 * 
 */

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
#include "../include/rdt.hpp"
#include "stdio.h"
int main(){
    char buf[1024];
    rdt_t rdt;
    printf("sizeof(rdt_t): %u\n", sizeof(rdt_t));
    print_pkt(&rdt);
    uint8_t data[4] = {0x0, 0x1, 0x2, 0x3};
    make_pkt(&rdt, 0, 0, data, sizeof(data));
    print_pkt(&rdt);
    // rdt.flag++;
    if(not_corrupt(&rdt)){
        printf("YES\n");
    }else{
        printf("NO\n");
    }
    print_pkt(&rdt);
}
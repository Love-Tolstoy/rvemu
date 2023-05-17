// #include <stdio.h>
#include "rvemu.h"

int main(int argc, char *argv[]){
    // printf("Hello,World!\n");
    assert(argc > 1);
    machine_t machine = {0};
    machine_load_program(&machine, argv[1]);  // 不传第0个是因为第0个事可执行文件本身
    printf("entry: %lx\n", machine.mmu.entry);
    return 0;
}
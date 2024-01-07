#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

//treba lock tokom swapovanja sa nekom promenjivom za yield

frameDsc frameDescTable[NUMBER_OF_FRAMES];
kernelLoaded = 0;
globalYieldLock = 0;


//Za lakse racunanje blok 0 predstavlja blok 0, 1, 2, 3  zajedno
static int diskList[NUMBER_OF_FRAMES] = {0};
static int head = 0;

int getFrameNumber(uint64 pa){
    return (pa - KERNBASE)/PGSIZE;
}

int dalloc(){
    //first time init
    if(diskList[head] == 0){
        diskList[head] = head + 1;
    }

    int ret = head;

    head = diskList[head];
    return ret;
}

void dfree(int blkNum){
    diskList[blkNum] = head;
    head = blkNum
}

void write_disk(int blkNum, uchar* data, int busy_wait){
    for(int currBlock = blkNum * 4;currBlock < blkNum*4 + 4; currBlock++){
        write_block(currBlock, data, busy_wait);
        data = data + 1024;
    }

}
void read_disk(int blkNum, uchar* data, int busy_wait){
    for(int currBlock = blkNum * 4;currBlock < blkNum*4 + 4; currBlock++){
        read_block(currBlock, data, busy_wait);
        data = data + 1024;
    }
}

void swapIn(pte_t *pte;){
    globalLock++;

    globalLock--;

}
void swapOut(){
    globalLock++;

    globalLock--;
}

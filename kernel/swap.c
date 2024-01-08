#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

frameDsc frameDescTable[NUMBER_OF_FRAMES];
int kernelLoaded = 0;
int globalYieldLock = 0;
void* swapBuffer;

//Za lakse racunanje blok 0 predstavlja blok 0, 1, 2, 3  zajedno
static int diskList[NUMBER_OF_FRAMES] = {0};
static int head = 0;

int getFrameNumber(uint64 pa){
    return (pa - KERNBASE)/PGSIZE;
}

void clearFrameDesc(uint64 pa){
    int frameNum = getFrameNumber(pa);
    frameDescTable[frameNum].pte = 0;
    frameDescTable[frameNum].restrictedSwap = 0;
    frameDescTable[getFrameNumber((uint64)pa)].refHistory = HIGHEST_BIT;
}

void initDisk(){
    head = 0;
    for(int i = 0; i < NUMBER_OF_FRAMES; i++){
        diskList[i] = i+1;
    }
    diskList[NUMBER_OF_FRAMES - 1] = -1;
}

int dalloc(){
    int ret = head;
    if(ret == -1) return -1;
    head = diskList[head];
    return ret;
}

void dfree(int blkNum){
    diskList[blkNum] = head;
    head = blkNum;
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

void updateRefBits(){
    globalYieldLock++;
    //printf("update");
    for(int i = 0; i < NUMBER_OF_FRAMES; i++){
        if (frameDescTable[i].pte == 0) continue;
        uint64 accessBit = *frameDescTable[i].pte & PTE_A;
        frameDescTable[i].refHistory >>= 1;
        if(accessBit) frameDescTable[i].refHistory |= HIGHEST_BIT;
        *frameDescTable[i].pte = ~PTE_A & *frameDescTable[i].pte; //clear A bit
    }
    globalYieldLock--;
}

int totalWorkingSet = 0;
void thrashing(){
    //globalYieldLock++;
    static int suspendedCnt = 0;
    totalWorkingSet = 0;

    for(int i = 0; i < NUMBER_OF_FRAMES; i++){
        if (frameDescTable[i].pte == 0) continue;
        if(frameDescTable[i].refHistory & 0xFFFFF00000000000) totalWorkingSet++;
    }
    //if(totalWorkingSet > 100) printf("totalWorkingSet: %d\n", totalWorkingSet);
    if(totalWorkingSet > NUMBER_OF_FRAMES * 7/10) {
        setSuspended(myproc());
        suspendedCnt++;
        //printf("Suspended\n");
    }
    else if(suspendedCnt > 0){
        suspendedCnt -= unsuspend();
        //printf("Unsuspended\n");
    }
    //globalYieldLock--;
}

//LRU
pte_t* getVictim(){
    globalYieldLock++;
    //printf("victim\n");
    int index = -1;
    uint64 minValue = 0xFFFFFFFFFFFFFFFF;
    for(int i = 0; i < NUMBER_OF_FRAMES; i++){
        if (frameDescTable[i].restrictedSwap || frameDescTable[i].pte == 0 || !(PTE_U & *frameDescTable[i].pte)) continue;
        if(frameDescTable[i].refHistory < minValue){
            minValue = frameDescTable[i].refHistory;
            index = i;
        }
    }
    globalYieldLock--;
    return frameDescTable[index].pte;
}

//FIFO
//pte_t * getVictim(){
//    static int hand = 0;
//
//    while(1){
//        if(!frameDescTable[hand].restrictedSwap && frameDescTable[hand].pte && (*frameDescTable[hand].pte & PTE_U )){
//            int temp = hand;
//            hand++;
//            if(hand > NUMBER_OF_FRAMES) hand = 0 ;
//            return frameDescTable[temp].pte;
//        }
//
//        hand++;
//        if(hand > NUMBER_OF_FRAMES) hand = 0 ;
//    }
//
//}

void swapIn(pte_t *pte){
    globalYieldLock++;

    //extract disk page
    uint64 diskBlk = (*pte) >> 10;
    dfree(diskBlk);
    read_disk(diskBlk, (uchar*)swapBuffer, 1);
    void* pa = kalloc();
    memmove(pa, swapBuffer, PGSIZE);

    *pte = (*pte & 0x3FF) | PA2PTE(pa);
    *pte = *pte & ~PTE_RSW1; // rws = 0
    *pte = *pte | PTE_V; // valid = 1

    frameDescTable[getFrameNumber((uint64)pa)].pte = pte;
    frameDescTable[getFrameNumber((uint64)pa)].refHistory = HIGHEST_BIT;
    //frameDescTable[getFrameNumber((uint64)pa)].frameOwner = myproc();

    globalYieldLock--;
}
void* swapOut(){
    globalYieldLock++;

    pte_t* victim = getVictim();
    if(victim == 0) printf("LOSE VRACA VICTIMA");
    int blockNum = dalloc();
    if(blockNum == -1) { //pun disk
        //printf("PUN DISK");
        globalYieldLock--;
        return 0;
    }
    uint64 pa = PTE2PA(*victim);

    *victim = *victim | PTE_RSW1; // rsw = 1
    *victim = *victim & (~PTE_V); // valid = 0
    //Upisuje broj diska
    *victim &= 0x3FF;
    *victim |= blockNum << 10;

    write_disk(blockNum, (uchar*)pa, 1); //upise na disk

    frameDescTable[getFrameNumber(pa)].pte = 0;
    frameDescTable[getFrameNumber(pa)].refHistory = HIGHEST_BIT;
    //frameDescTable[getFrameNumber((uint64)pa)].frameOwner = 0;

    //printf("SWAP OUT\n");
    sfence_vma(); //TLB FLUSH
    globalYieldLock--;
    return (void*)pa;
}

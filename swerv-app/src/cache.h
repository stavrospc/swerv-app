#ifndef CACHE_H
#define CACHE_H

// #include "../src/printf.c"

// cache values
// -> Values taken from paper on SweRV par. C and 
// https://en.wikipedia.org/wiki/CPU_cache#Cache_entry_structure
#define L1_SETS 128
#define L1_SET_BITS 7 // note: this is log2Ceil(L1_SETS)
#define L1_WAYS 4 //note: this looks like there are 4 ways
#define L1_BLOCK_SZ_BYTES 64
#define L1_BLOCK_BITS 6 // note: this is log2Ceil(L1_BLOCK_SZ_BYTES)
#define L1_SZ_BYTES (L1_SETS*L1_WAYS*L1_BLOCK_SZ_BYTES)
#define FULL_MASK 0xFFFFFFFFFFFFFFFF
#define OFF_MASK (~(FULL_MASK << L1_BLOCK_BITS))
#define TAG_MASK (FULL_MASK << (L1_SET_BITS + L1_BLOCK_BITS))
#define SET_MASK (~(TAG_MASK | OFF_MASK))

/* ----------------------------------
 * |                  Cache address |
 * ----------------------------------
 * |       tag |      idx |  offset |
 * ----------------------------------
 * | 63 <-> 14 | 13 <-> 6 | 5 <-> 0 |
 * ----------------------------------
 */

// setup array size of cache to "put" in the cache on $ flush
// guarantees contiguous set of addrs that is at least the sz of cache
// 5 so that you can hit more
uint8_t dummyMem[5 * L1_SZ_BYTES];

/**
 * Flush the cache of the address given since RV64 does not have a
 * clflush type of instruction. Clears any set that has the same idx bits
 * as the address input range.
 *
 * Note: This does not work if you are trying to flush dummyMem out of the
 * cache.
 *
 * @param addr starting address to clear the cache
 * @param sz size of the data to remove in bytes
 */
void flushCache(uint32_t addr, uint32_t sz){
    printf("Flushed addr(0x%x) tag(0x%x) set(0x%x) off(0x%x) sz(%d)\n",
            addr, 
            ((uint64_t)addr & TAG_MASK) >> (L1_SET_BITS + L1_BLOCK_BITS), 
            (addr & SET_MASK) >> L1_BLOCK_BITS, 
            addr & OFF_MASK,
            sz);

    // find out the amount of blocks you want to clear
    uint32_t numSetsClear = sz >> L1_BLOCK_BITS;
    if ((sz & OFF_MASK) != 0){
        numSetsClear += 1;
    }
    if (numSetsClear > L1_SETS){
        // flush entire cache with no rollover (makes the function finish faster) 
        numSetsClear = L1_SETS;
    }

    // printf("numSetsClear(%d)\n", numSetsClear);

    // temp variable used for nothing
    uint8_t dummy = 0;

    // this mem address is the start of a contiguous set of memory that will fit inside of the
    // cache
    // thus it has the following properties
    // 1. dummyMem <= alignedMem < dummyMem + sizeof(dummyMem)
    // 2. alignedMem has idx = 0 and offset = 0
    uint64_t alignedMem = (((intptr_t)&dummyMem) + L1_SZ_BYTES) & TAG_MASK;
    // printf("alignedMem(0x%x)\n", alignedMem);

    for (uint32_t i = 0; i < numSetsClear; ++i){
        // offset to move across the sets that you want to flush
        uint32_t setOffset = (((addr & SET_MASK) >> L1_BLOCK_BITS) + i) << L1_BLOCK_BITS;
        // printf("setOffset(0x%x)\n", setOffset);

        // since there are L1_WAYS you need to flush the entire set
        for(uint32_t j = 0; j < 4*L1_WAYS; ++j){
            // offset to reaccess the set
            uint32_t wayOffset = j << (L1_BLOCK_BITS + L1_SET_BITS);
            // printf("wayOffset(0x%x)\n", wayOffset);

            // evict the previous cache block and put in the dummy mem
            dummy = *((uint8_t*)(alignedMem + setOffset + wayOffset));
            // printf("evict read(0x%x)\n", alignedMem + setOffset + wayOffset);
        }
    }
}

#endif

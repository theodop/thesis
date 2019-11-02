#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
  time_t t;
  srand((unsigned) time(&t));

  unsigned int generated1 = rand() % 65535;

  printf("Generated 1: %d\n", generated1);
  unsigned int b11 = generated1 & 0b0000000011111111;
  printf("Byte 1,1: %d\n", b11);
  unsigned int b12 = (generated1 & 0b1111111100000000) >> 8;
  printf("Byte 1,2: %d\n", b12);

  unsigned int b1scaled = (b11 + (b12 << 8))*0.5;

  unsigned int generated2 = rand() % 65535;

  printf("Generated 2: %d\n", generated2);
  unsigned int b21 = generated2 & 0b0000000011111111;
  printf("Byte 1,1: %d\n", b21);
  unsigned int b22 = (generated2 & 0b1111111100000000) >> 8;
  printf("Byte 1,2: %d\n", b22);
  unsigned int b2scaled = (b21 + (b22 << 8))*0.5;

  unsigned int added = b1scaled + b2scaled;

  if (added > 65535) {
    printf("Clipped\n");
    added = 65535;
  }

  printf("Result: %d\n", added);
  unsigned int added1 = added & 0b0000000011111111;
  printf("Added byte1: %d\n", added1);
  unsigned int added2 = (added & 0b1111111100000000) >> 8;
  printf("Added byte2: %d\n", added2);

  return 0;
}

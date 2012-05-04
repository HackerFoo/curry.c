#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>

/*
# must turn off selinux protection
setsebool -P allow_execheap=1

gcc -Os curry.c -o curry

LIST OF COMPILER ASSUMPTIONS

- functions are arranged in memory as they are in the code
- replaced constants occur before any other matching sequence of bytes
- replaced constants occur in the instructions in as a single assignment
- modified code is relocatable
- no W^X

*/

typedef void *ptr_t;

#define DEBUG

// constants to be replaced
#define REPLACE_ME_F1_F 0x1337bead1337bead
#define REPLACE_ME_F2_F 0xba5e1eadba5e1ead
#define REPLACE_ME_F2_X 0xf1bea493

// offsets of constants in function code
int f1_f = 0;
int f2_f = 0;
int f2_x = 0;

void print_hex(unsigned char *n, unsigned char *b, unsigned int s) {
  int i;
  printf("%s[%d] =\n [ ", n, s);
  for(i = 0; i < s - 1; i++) {
    if(i % 8 == 0 && i) printf("\n   ");
    printf("0x%.2x, ", *(b+i));
  }
  printf("0x%.2x ]\n", *(b+s-1));
}

// functions to curry

int op_add(int x, int y) {
  return x + y;
}

int op_sub(int x, int y) {
  return x - y;
}

int op_mul(int x, int y) {
  return x * y;
}

int op_div(int x, int y) {
  return x / y;
}

int op_zero(int x, int y) {
  return 0;
}

// function template for function returned after curried function is called with first argument
// calls helper function to minimize size
// this function should never be actually called, and will segfault
int f2(int y) {
  int (*f)(int, int) = (int (*)(int, int))REPLACE_ME_F2_F;
  int x = REPLACE_ME_F2_X;
  return f(x, y);
}
void end_f2() {} // address after end of f2

// pointer to f1 helper function
// avoids relative jump, and is altered after calculating offsets
int (*(*_f1) (int (*)(int, int), int))(int);

// f1 helper function without offset computation
int (*__f1_no (int (*f)(int, int), int x)) (int) {
  intptr_t size = (intptr_t) end_f2 - (intptr_t) f2;
  unsigned char *c = memcpy(valloc(size), f2, size);

  mprotect(c, size, PROT_READ | PROT_WRITE | PROT_EXEC);

  *(ptr_t*)(c+f2_f) = (ptr_t)f;
  *(int*)(c+f2_x) = (int)x;

#ifdef DEBUG
  print_hex("c", c, size);
#endif

  return (int (*)(int))c;
}

// f1 helper function with offset computation
int (*__f1_o (int (*f)(int, int), int x)) (int) {
  intptr_t size = (intptr_t) end_f2 - (intptr_t) f2;
  unsigned char *c = memcpy(valloc(size), f2, size);

  mprotect(c, size, PROT_READ | PROT_WRITE | PROT_EXEC);

  // find offsets
  int i;
  for(i = 0; i < size - sizeof(ptr_t); i++) {
    if(*(ptr_t*)(c+i) == (ptr_t)REPLACE_ME_F2_F) f2_f = i;
    if(*(int*)(c+i) == REPLACE_ME_F2_X) f2_x = i;
  }

  *(ptr_t*)(c+f2_f) = (ptr_t)f;
  *(int*)(c+f2_x) = (int)x;

#ifdef DEBUG
  printf("f2_f = %d\n", f2_f);
  printf("f2_x = %d\n", f2_x);
  print_hex("c", c, size);
#endif

  _f1 = &__f1_no; // turn off offset computation

  return (int (*)(int))c;
}

int (*(*_f1) (int (*)(int, int), int))(int) = &__f1_o; // initial f1 helper

// function template for function returned from curry
// calls f1 helper function to minimize size
// this function should never be actually called, and will segfault
int (*f1 (int x))(int) {
  int (*f)(int, int) = (int (*)(int, int))REPLACE_ME_F1_F;
  return _f1(f, x);
}
void end_f1() {} // address after end of f1

// pointer to curry function
// altered after calculating offsets
int (*(*(*curry) (int (*f) (int, int))) (int)) (int);

// curry without offset calculation
int (*(*__curry_no (int (*f) (int, int))) (int)) (int) {
  intptr_t size = (intptr_t) end_f1 - (intptr_t) f1;
  unsigned char *c = memcpy(valloc(size), f1, size);

  mprotect(c, size, PROT_READ | PROT_WRITE | PROT_EXEC);

  *(ptr_t*)(c+f1_f) = (ptr_t)f; // set the function call
  
#ifdef DEBUG
  print_hex("c", c, size);
#endif

  return (int (*(*) (int)) (int))c;
}

// curry with offset calculation
int (*(*__curry_o (int (*f) (int, int))) (int)) (int) {
  intptr_t size = (intptr_t) end_f1 - (intptr_t) f1;
  unsigned char *c = memcpy(valloc(size), f1, size);

  mprotect(c, size, PROT_READ | PROT_WRITE | PROT_EXEC);

  int i;
  for(i = 0; i < size; i++) {
    if(*(intptr_t*)(c+i) - (intptr_t)REPLACE_ME_F1_F == 0) f1_f = i;
    //printf("c+%d = 0x%.16lx =? 0x%.16lx : %d\n", i, *(intptr_t*)(c+i), REPLACE_ME_F1_F, *(intptr_t*)(c+i) - (intptr_t)REPLACE_ME_F1_F);
  }

  *(ptr_t*)(c+f1_f) = (ptr_t)f; // set the function call
  
#ifdef DEBUG
  printf("f1_f = %d\n", f1_f);
  print_hex("c", c, size);
#endif

  curry = &__curry_no;

  return (int (*(*) (int)) (int))c;
}

int (*(*(*curry) (int (*f) (int, int))) (int)) (int) = &__curry_o; // initial curry function

int main(int argc, char *argv[]) {
  int (*f) (int, int); // holds function to be curried
  char *op_str;
  if(argc < 5) return -1;

  printf("pc = 0lx%.16x\n", &&pc);
pc:

  // select function
  switch(argv[1][0]) {
    case 'a': f = &op_add; op_str = "op_add"; break;
    case 's': f = &op_sub; op_str = "op_sub"; break;
    case 'm': f = &op_mul; op_str = "op_mul"; break;
    case 'd': f = &op_div; op_str = "op_div"; break;
    default: f = &op_zero; op_str = "op_zero"; break;
  }

  // read arguments
  int a = atoi(argv[2]);
  int b = atoi(argv[3]);
  int x = atoi(argv[4]);

  int (*(*c1) (int)) (int) = curry(f);
  int (*c2)(int) = c1(a);
  int r1 = c2(b);
  int r2 = c2(x);

  printf("c1 = curry(%s) = 0x%.8x\n", op_str, c1);
  printf("c2 = c1(%d) = 0x%.8x\n", a, c2);
  printf("r1 = c2(%d) = %d\n", b, r1);
  printf("r2 = c2(%d) = %d\n", x, r2);

  return 0;
}

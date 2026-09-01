#include <stdio.h>
#include <stdlib.h>

#define str(x) #x
#define max(a, b) ((a) > (b)) ? (a) : (b))
#define min(a, b) ((a) < (b)) ? (a) : (b))
#define endl printf("\n")
#define null NULL

void test_macro(void) {
  printf("%s\n", str(123));
  printf("%d\n", max(1, 2));
  printf("%d\n", min(1, 2));
}

int main(void) {
  printf("Hello World");
  endl;

  test_macro();
  
  return 0;
}

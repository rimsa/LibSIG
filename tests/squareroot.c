#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(int argc, char* argv[]) {
  int n = atoi(argv[1]);

  void* handle = dlopen("/lib/x86_64-linux-gnu/libm.so.6", RTLD_NOW);
  double (*sqrt)(double) = dlsym(handle, "sqrt");
  double s = (*sqrt)(n);
  dlclose(handle);

  printf("√%d = %g\n", n, s);
  return 0;
}

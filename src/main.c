#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include "../include/raspberry.h"

void urg_signal_handler(int number) {
  exit(0);
}

int main() {

  int pid = fork();

  if (pid < 0) {
    perror("fork");
    exit(-1);
  }

  if (pid > 0) {
    exit(0);
  }

  setsid();

  signal(SIGURG, urg_signal_handler);

  int chdir_res;
  chdir_res = chdir("/");
  if (chdir_res < 0) {
    perror("chdir");
    exit(1);
  }

  fclose(stdout);
  fclose(stdin);
  fclose(stderr);

  while(1) {}

  return 0;
}

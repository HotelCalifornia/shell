#include <cstdio>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "shell.hh"

#include "y.tab.hh"

int yyparse(void);

void Shell::prompt(bool newline) {
  if (isatty(0)) {
    if (newline) printf("\n");
    printf("myshell>");
    fflush(stdout);
  }
}

extern "C" void handle_int(int) {
  // do nothing
  // fprintf(stderr, "\nreceived signal %d (%s)\n", sig, strsignal(sig));
  // printf("[interrupted] ");

  Shell::prompt(true);
}

int main() {
  // yydebug = 1;
  // handle SIGINT
  struct sigaction sa;
  sa.sa_handler = handle_int;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL)) {
    perror(strerror(errno));
    exit(-1);
  }

  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;

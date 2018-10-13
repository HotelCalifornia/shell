#include <cstdio>

#include <signal.h>
#include <unitsd.h>

#include "shell.hh"

#include "y.tab.hh"

int yyparse(void);

void Shell::prompt() {
  if (isatty(0)) {
    printf("myshell>");
    fflush(stdout);
  }
}

extern "C" void handle_int(int sig) {
  fprintf(stderr, "\nreceived signal %d (%s)\n", sig, strsignal(sig));
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

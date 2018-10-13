#include <cstdio>

#include <unistd.h>

#include "shell.hh"

int yyparse(void);

void Shell::prompt() {
  printf("myshell>");
  fflush(stdout);
}

int main() {
  yydebug = 1;
  if (isatty(0)) Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;

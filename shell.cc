#include <string>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "shell.hh"

#include "y.tab.hh"

extern void lsource(std::string* fname);

int yyparse(void);

static bool _is_subshell = false;

bool Shell::is_subshell() { return _is_subshell; }

void Shell::prompt(bool newline) {
  if (isatty(0) && !getenv("SOURCE_COMMAND") && !getenv("SUBSHELL")) {
    if (newline) printf("\n");
    printf("myshell>");
    fflush(stdout);
  }
}

void Shell::source(std::string* fname, bool needresume) {
  setenv("SOURCE_COMMAND", "1", false);
  lsource(fname);
  unsetenv("SOURCE_COMMAND");

  // resume parse loop. needresume is true unless sourcing .shellrc
  if (needresume){
    Shell::prompt();
    yyparse();
  }
}

extern "C" void handle_int(int) {
  // do nothing
  // fprintf(stderr, "\nreceived signal %d (%s)\n", sig, strsignal(sig));
  // printf("[interrupted] ");

  Shell::prompt(true);
}

int main(int argc, char** argv) {
  if (argc > 1) {
    if (!strcmp(argv[1], "subshell")) {
      _is_subshell = true;
    }
  }
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

  // source shellrc
  std::string sourcedir = "";
  if (getenv("HOME")) sourcedir = getenv("HOME");
  else sourcedir = ".";

  sourcedir += "/.shellrc";

  if (access(sourcedir.c_str(), R_OK) != -1) {
    // don't need to resume parse loop because we'll fall through
    Shell::source(&sourcedir, false);
  } else {
    fprintf(stderr, "error: source %s: %s\n", sourcedir.c_str(), strerror(errno));
  }

  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;

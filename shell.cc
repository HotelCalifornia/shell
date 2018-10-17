#include <string>
#include <sstream>
#include <iterator>
#include <iostream>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell.hh"

#include "y.tab.hh"

void lsource(std::string* fname);

void myunputc(int c);

int yyparse(void);

void Shell::do_subshell(std::string *input) {
  // printf("dbg: in subshell parse\n");
  /* strip '$(' and ')' */
  auto cmd = std::string(input->begin() + 2, input->end() - 1);

  // printf("\tcleaned subshell cmd: %s\n", cmd.c_str());

  pid_t pid;

  int dfin = dup(STDIN_FILENO);
  int dfout = dup(STDOUT_FILENO);

  int cmdPipe[2]; pipe(cmdPipe);
  int outPipe[2]; pipe(outPipe);

  dprintf(cmdPipe[1], "%s\nexit\n", cmd.c_str());
  close(cmdPipe[1]);

  dup2(cmdPipe[0], STDIN_FILENO);
  dup2(outPipe[1], STDOUT_FILENO);
  if ((pid = fork()) == -1) {
    perror("fatal: fork\n");
    exit(-1);
  } else if (pid == 0) { /* child */
    close(cmdPipe[1]);
    close(outPipe[0]);
    execlp("/proc/self/exe", "subshell");
  } else { /* parent */
    waitpid(-1, NULL, 0);

    dup2(dfin, STDIN_FILENO);
    dup2(dfout, STDOUT_FILENO);
    close(cmdPipe[0]);
    close(outPipe[1]);
    /* wait for child proc to finish in order to ensure all the data we want is avail */
    std::string subshell_out = "";
    char c;
    while (read(outPipe[0], &c, 1)) {
      if (c == EOF) break;
      subshell_out += c;
    }
    // std::cout << subshell_out << std::endl;
    std::string processed_subshell_out = "";
//XXX: uncomment if you want this shell to be more robust than /bin/sh
#if 0
    // preprocess string: quote lines with spaces and convert newlines to spaces
    std::stringstream ss(subshell_out);
    std::string line;
    while (std::getline(ss, line, '\n')) {
      if (std::find(line.begin(), line.end(), ' ') != line.end()) {
        line.insert(line.begin(), '\"');
        line.push_back('\"');
      }
      processed_subshell_out += line + ' ';
    }
#endif
    close(outPipe[0]);
    // replace newlines with spaces

    for (auto c : subshell_out) {
      if (c == '\n') processed_subshell_out += ' ';
      else processed_subshell_out += c;
    }
    // processed_subshell_out.insert(processed_subshell_out.begin(), '\"');
    // processed_subshell_out.push_back('\"');
    // printf("\tsubshell output: %s\n", subshell_out.c_str());

    std::for_each(
      processed_subshell_out.rbegin(),
      processed_subshell_out.rend(),
      [](auto c) { myunputc(c); }
    );
  }
}

static bool _is_subshell = false;

bool Shell::is_subshell() { return _is_subshell; }

void Shell::prompt(bool newline) {
  if (isatty(0) && !getenv("SOURCE_COMMAND") && !Shell::is_subshell()) {
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

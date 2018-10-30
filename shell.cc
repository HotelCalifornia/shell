#include <string>
#include <sstream>
#include <iterator>
#include <iostream>

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell.hh"

#include "y.tab.hh"

void lsource(std::string* fname);

void myunputc(int c);

int yyparse(void);

static char* subshell_argv[3] = {(char*)"/proc/self/exe", (char*)"subshell", NULL};

void Shell::do_subshell(std::string* input) {
  /* strip '$(' and ')' */
  auto cmd = std::string(input->begin() + 2, input->end() - 1);

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
    execvp(subshell_argv[0], subshell_argv);
    fprintf(stderr, "%s\n", strerror(errno));
    _exit(1);
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
    std::string processed_subshell_out = "";
    close(outPipe[0]);

    // replace newlines with spaces and escape unescaped characters
    for (auto c : subshell_out) {
      if (c == '\n') processed_subshell_out += ' ';
      else {
        switch (c) {
          case '\\':
          case '&':
          case '>':
          case '<':
          case '|':
          case '\'':
          case '\"':
            processed_subshell_out += '\\' + c;
            break;
          default: processed_subshell_out += c;
        }
      }
    }
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

    if (getenv("ON_ERROR")) {
      if (getenv("?")) {
        if (std::stoi(getenv("?"))) std::cout << getenv("ON_ERROR") << std::endl;
      }
    }

    std::string ps1 = "myshell>";
    if (getenv("PROMPT")) ps1 = getenv("PROMPT");
    std::cout << ps1;
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
  Shell::prompt(true);
}

// for debug:
// extern "C" void handle_segv(int) {
//   std::cerr << "trap segv " << getpid() << std::endl;
//   while (true) {}
// }

int main(int argc, char** argv) {
  if (argc > 1) {
    if (!strcmp(argv[1], "subshell")) {
      _is_subshell = true;
    }
  }
  // handle SIGINT
  struct sigaction sa;
  sa.sa_handler = handle_int;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL)) {
    perror(strerror(errno));
    exit(-1);
  }
  // for debug:
  // struct sigaction sad;
  // sad.sa_handler = handle_segv;
  // sigemptyset(&sad.sa_mask);
  // sad.sa_flags = 0;
  // sigaction(SIGSEGV, &sad, NULL);
  // yydebug = 1;

  // set SHELL builtin var
  char* tmp = realpath(argv[0], NULL);
  if (tmp) {
    setenv("SHELL", strdup(tmp), true);
    free(tmp);
  }
  // set $ builtin var
  pid_t pid = getpid();
  setenv("$", std::to_string(pid).c_str(), true);

  if (!Shell::is_subshell()) {
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
  }
  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;

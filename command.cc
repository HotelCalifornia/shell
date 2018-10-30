/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */
#define _XOPEN_SOURCE
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <regex>

#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h> // strsignal
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.hh"
#include "shell.hh"

#define HANDLE_ERRNO \
  std::cerr << "error: " << strerror(errno) << std::endl;

#define CLEAR_AND_RETURN \
  clear(); \
  return;

extern char** environ;

extern "C" void handle_chld(int) {
  // special thanks to https://stackoverflow.com/a/2378036/3681958
  pid_t p;
  int status;

  while ((p = waitpid(-1, &status, WNOHANG)) != -1) {
    std::cout << "[" << p << "]" << " exited with code " << status << std::endl;
  }
}

Command::Command() {
  // Initialize a new vector of Simple Commands
  _simpleCommands = std::vector<SimpleCommand *>();

  _outFile = NULL;
  _inFile = NULL;
  _errFile = NULL;
  _background = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
  // add the simple command to the vector
  _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
  // deallocate all the simple commands in the command vector
  for (auto simpleCommand : _simpleCommands) {
      delete simpleCommand;
  }

  // remove all references to the simple commands we've deallocated
  // (basically just sets the size to 0)
  _simpleCommands.clear();

  // delete only outfile if outfile and errfile are equivalent, otherwise delete them individually
  if ((_outFile || _errFile) && _outFile == _errFile) delete _outFile;
  else if (_outFile) delete _outFile;
  else if (_errFile) delete _errFile;

  if (_inFile) delete _inFile;

  _inFile = NULL;
  _outFile = NULL;
  _errFile = NULL;

  _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

/**
 * expand tilde in arg to home directory
 */
void Command::tilde(std::string* arg) {
  while (true) { // tilde expansion
    auto h = std::find(arg->begin(), arg->end(), '~');
    if (h == arg->end()) break;
    if (arg->size() == 1 || *(h + 1) == '/') { // standalone or followed by /
      // expand to current user home dir
      struct passwd* pwd = getpwnam(getenv("USER"));
      if (pwd) {
        arg->replace(h, h + 1, pwd->pw_dir);
      } else break; // no user found, don't worry about it
    } else { // followed by word, assumed to be username
      auto e = std::find(h, arg->end(), '/');
      std::string uname(h + 1, e);
      struct passwd* pwd = getpwnam(uname.c_str());
      if (pwd) {
        arg->replace(h, e, pwd->pw_dir);
      } else break; // no user found, don't worry about it
    }
  }
}

/**
 * expand environment variables in arg
 */
void Command::env(std::string* arg) {
  while (true) { // environment variable expansion
    auto b = std::find(arg->begin(), arg->end(), '$');
    auto e = std::find(b, arg->end(), '}');
    // invalid expansion syntax; ignore
    if (b == arg->end() || *(b + 1) != '{' || e == arg->end()) break;
    auto tvar = getenv(std::string(b + 2, e).c_str());
    std::string var = !tvar ? "" : tvar;
    arg->replace(b, e + 1, var);
  }
}

// NOTE: wildcard related functions were adapted into C++ from the relevant
//       implementation provided by the Golang standard library

/**
 * clean a path to prepare for wildcard matching.
 *
 * if there is no path specified, return the current directory "."
 * if the path is just "/", do nothing
 * otherwise, strip a trailing slash from the path
 */
std::string Command::prep_wildcard_path(std::string path) {
  if (path == "") return ".";
  else if (path == "/") return path;
  else return path.substr(0, path.size() - 1);
}

/**
 * utility function to determine if a given string has any wildcard characters
 * in it
 */
bool Command::has_wildcard_pattern(std::string arg) {
  return std::regex_search(arg, std::regex("\\*")) ||
         std::regex_search(arg, std::regex("\\?"));
}

/**
 * split a path into "path" and "file" components
 *
 * the "path" component is the whole path until the final pathsep
 * the "file" component is everything after the last pathsep
 */
std::tuple<std::string, std::string>
Command::get_path_and_file_component(std::string path) {
  auto last_sep = path.rfind('/');
  return { path.substr(0, last_sep + 1), path.substr(last_sep + 1, path.size()) };
}

/**
 * utility function to convert a wildcard pattern into a std::regex
 */
std::regex Command::wildcard_pattern_to_regex(std::string pattern) {
  std::string t = "";
  for (auto c : pattern) {
    switch (c) {
      case '.': t += "\\."; break;
      case '*': t += ".*"; break;
      case '?': t += "."; break;
      default: t.push_back(c);
    }
  }

  return std::regex(t);
}

/**
 * utility function to determine whether a given name matches a pattern
 *
 * if the pattern does not begin with ".", names that begin with "." are ignored
 */
bool Command::wildcard_match(std::string name, std::string pattern) {
  if (pattern.front() != '.') { // only match names beginning with . if explicitly specified
    return name.front() != '.' &&
           std::regex_match(name, wildcard_pattern_to_regex(pattern));
  } else return std::regex_match(name, wildcard_pattern_to_regex(pattern));
}

/**
 * utility function for performing wildcard matches
 */
std::vector<std::string> Command::wildcard_helper(
  std::string dir,
  std::string pattern,
  std::optional<std::vector<std::string>> matches) {

  // set up the vector that will be returned. if the `matches` argument was
  // specified, start with that. otherwise start with an empty vector
  std::vector<std::string> m;
  if (matches) m = *matches;
  else m = {};

  // check whether the specified directory is actually a directory. if not,
  // return an empty vector
  struct stat statbuf;
  stat(dir.c_str(), &statbuf);

  if (!S_ISDIR(statbuf.st_mode)) return {};
  DIR* d = opendir(dir.c_str());

  // read through the directory's entries and save the names into another vector
  struct dirent* ent;
  std::vector<std::string> names;
  while ((ent = readdir(d))) {
    names.push_back(ent->d_name);
  }
  closedir(d);

  // for each name, test whether it matches the given pattern. if it does, add
  // it to the result vector `m`
  for (auto name : names) {
    if (wildcard_match(name, pattern)) {
      m.push_back(
        dir == "." ?
          name :
          dir.back() == '/' ?
            dir + name :
            dir + "/" + name
      );
    }
  }

  // sort the results in ascending order
  std::sort(
    m.begin(),
    m.end(),
    // compare while ignoring case
    [](std::string a, std::string b) {
      // XXX: uncomment to make more like bash, leave commented to make like /bin/sh
      // std::transform(a.begin(), a.end(), a.begin(), ::tolower);
      // std::transform(b.begin(), b.end(), b.begin(), ::tolower);
      return a < b;
    }
  );
  return m;
}

/**
 * main driver function for performing wildcard matching
 */
std::vector<std::string> Command::wildcard(std::string arg) {
  // if the argument has no wildcard characters, return no matches
  if (!has_wildcard_pattern(arg)) return {};

  // extract the "path" and "file" components from the argument
  std::string dir, file;
  std::tie(dir, file) = get_path_and_file_component(arg);
  // clean the "path" portion in preparation for matching
  dir = prep_wildcard_path(dir);
  // if the "path" has no wildcard characters, there's no need to recurse. simply
  // return results in "path" that match "file"
  if (!has_wildcard_pattern(dir)) {
    return wildcard_helper(dir, file, std::nullopt);
  }
  // make sure that "path" is not equivalent to the whole of `arg`. if it was,
  // we could end up recursing infinitely. it's clear at this point that if
  // "path" was going to diverge from `arg` it would have done so by now, so we
  // can just assume that there are no matches to be found.
  if (dir == arg) return {};

  // set up vectors for output
  std::vector<std::string> m, matches;
  // recurse to process "path" component
  m = wildcard(dir);
  // for each match returned by recursion, get a list of file matches from the
  // helper function
  for (auto d : m) {
    // only update the master matches vector if there were matches returned by
    // the helper. without this check, the master vector could be overwritten
    // by empty lists, which is undesirable
    auto intermediary_matches = wildcard_helper(d, file, matches);
    if (intermediary_matches.size()) matches = intermediary_matches;
  }
  return matches;
}

/**
 * intermediary function to transform an argument with wildcards in-place. if no
 * matches were found, do nothing
 */
void Command::expand_wildcard(std::string* arg) {
  auto wm = wildcard(*arg);
  if (wm.size() > 0) {
    std::string output = "";
    for (auto m : wm) {
      output += m;
      if (m != wm.back()) output += " ";
    }
    *arg = output;
  }
}

/**
 * expand tilde, environment variables, and wildcards
 */
void Command::expand() {
  for (auto cmd : _simpleCommands) {
    for (auto arg : cmd->_arguments) {
      if (arg) {
        tilde(arg);
        env(arg);
        if (has_wildcard_pattern(*arg))
          expand_wildcard(arg);
      }
    }
  }
}

/**
 * main driver for command execution
 */
void Command::execute() {
  // Don't do anything if there are no simple commands
  if (_simpleCommands.size() == 0) {
      Shell::prompt();
      return;
  }

  expand();

  // handle some builtin commands (cd, exit, setenv, unsetenv)
  auto tmpCmd = *(_simpleCommands[0]->_arguments[0]);
  if (tmpCmd == "cd") {
    std::string dir = "";
    // XXX: doesn't handle the case in which neither HOME nor OLDPWD are set
    if (_simpleCommands[0]->_arguments.size() < 2) { // no path specified, set to home
      dir = getenv("HOME");
    } else if (!strcmp(_simpleCommands[0]->_arguments[1]->c_str(), "-")) { // set to oldpwd
      auto tmp = getenv("OLDPWD");
      if (!tmp) tmp = getenv("HOME");
      dir = tmp;
    } else dir = *_simpleCommands[0]->_arguments[1];

    // set oldpwd before changing
    auto old = getcwd(NULL, 0);
    setenv("OLDPWD", old, true);
    delete old;

    int status = chdir(dir.c_str());
    if (status) {
      std::cerr << "cd: can't cd to " << dir << std::endl;
    }
    CLEAR_AND_RETURN
  } else if (tmpCmd == "exit") {
    if (isatty(0) && !Shell::is_subshell()) std::cout << "logout" << std::endl;
    clear();
    exit(0);
  } else if (tmpCmd == "setenv") {
    if (_simpleCommands[0]->_arguments.size() < 3 ||
        _simpleCommands[0]->_arguments.size() > 3 ||
        _simpleCommands.size() > 1) {
      std::cerr << "usage: setenv A B" << std::endl;
      CLEAR_AND_RETURN
    }
    if (setenv(_simpleCommands[0]->_arguments[1]->c_str(), _simpleCommands[0]->_arguments[2]->c_str(), true)) {
      HANDLE_ERRNO
    }
    CLEAR_AND_RETURN
  } else if (tmpCmd == "unsetenv") {
    if (_simpleCommands[0]->_arguments.size() < 2 ||
        _simpleCommands[0]->_arguments.size() > 2 ||
        _simpleCommands.size() > 1) {
      std::cerr << "usage: unsetenv A" << std::endl;
      CLEAR_AND_RETURN
    }
    // NOTE: alternate method: set value to "" (EDIT: passes the test case as-is so whatever)
    if (unsetenv(_simpleCommands[0]->_arguments[1]->c_str())) {
      HANDLE_ERRNO
    }
    CLEAR_AND_RETURN
  } else if (tmpCmd == "match") { //extra function for testing the output of wildcard matching
    if (_simpleCommands[0]->_arguments.size() < 2 ||
        _simpleCommands[0]->_arguments.size() > 2 ||
        _simpleCommands.size() > 1) {
      std::cerr << "usage: match pattern" << std::endl;
      CLEAR_AND_RETURN
    }
    std::vector<std::string> matches = wildcard(*_simpleCommands[0]->_arguments[1]);
    for (auto m : matches) std::cout << m << std::endl;
    CLEAR_AND_RETURN
  }

  pid_t pid;

  int ifd = STDIN_FILENO;
  int ofd = STDOUT_FILENO;
  int efd = STDERR_FILENO;

  int stdinfd = dup(ifd);
  int stdoutfd = dup(ofd);
  int stderrfd = dup(efd);

  int pipefd[2];
  for (auto cmd : _simpleCommands) {
    // special thanks to https://stackoverflow.com/questions/17630247/coding-multiple-pipe-in-c/17631589
    pipe(pipefd);

    // file redirection
    if (cmd == _simpleCommands.front()) { // first command, open input/error redirect file if necessary
      if (_inFile) ifd = open(_inFile->c_str(), O_RDONLY);
      int flags = O_CREAT | O_WRONLY;
      flags |= _e_append ? O_APPEND : O_TRUNC;
      if (_errFile) efd = open(_errFile->c_str(), flags, 0666);

      if (ifd < 0) {
        perror("input: bad file descriptor\n");
        exit(2);
      }
      if (efd < 0) {
        perror("fatal: creat error file\n");
        exit(2);
      }
    }
    // NOTE: separate clauses because sometimes there's only one cmd ;-)
    if (cmd == _simpleCommands.back()) { // last command, send output to redirect
      // only need to open output fd once if out and err go to same place (err already initialized)
      int flags = O_CREAT | O_WRONLY;
      flags |= _s_append ? O_APPEND : O_TRUNC;
      if ((_outFile || _errFile) && _outFile == _errFile) ofd = efd;
      else if (_outFile) ofd = open(_outFile->c_str(), flags, 0666);

      if (ofd < 0) {
        perror("fatal: creat output file\n");
        exit(2);
      }
    }

    // main execution + piping
    if ((pid = fork()) == -1) {
      perror("fatal: fork\n");
      exit(2);
    } else if (pid == 0) { // child proc
      // stdin from previous segment of pipe
      dup2(ifd, STDIN_FILENO);
      // stderr to specified fd
      dup2(efd, STDERR_FILENO);

      // if current command is not the last one, direct output to next segment
      if (cmd != _simpleCommands.back())
        dup2(pipefd[1], STDOUT_FILENO);
      else // otherwise (last command) send output to specified fd
        dup2(ofd, STDOUT_FILENO);

      close(pipefd[0]); // close input

      // handle builtins
      if (!strcmp(cmd->_arguments[0]->c_str(), "printenv")) {
        int i = 0;
        while (environ[i]) {
          std::string tmp(environ[i]);
          if (tmp[0] != '_' && tmp.find("_=") != 0) // skip _ env var
            std::cout << environ[i] << std::endl;
          i++;
        }
        exit(0);
      } else if (!strcmp(cmd->_arguments[0]->c_str(), "source")) {
        // pass
        std::cout << "source" << std::endl;
        exit(0);
      } else { // handle other commands
        // convert to const* char*
        // special thanks to https://stackoverflow.com/questions/48727690/invalid-conversion-from-const-char-to-char-const
        std::vector<char*> argv;
        for (auto arg : cmd->_arguments) argv.push_back(arg->data());
        argv.push_back(NULL);

        exit(execvp(argv[0], argv.data()));
      }
    } else { // parent
      close(pipefd[1]); // close segment output
      ifd = pipefd[0];
    }
  }
  // restore stdin, stdout, stderr
  dup2(stdinfd, STDIN_FILENO);
  dup2(stdoutfd, STDOUT_FILENO);
  dup2(stderrfd, STDERR_FILENO);

  // special print for backgrounded processes
  if (_background) {
    std::cout << "[1] " << pid << std::endl;
    // handle exit
    struct sigaction sa;
    sa.sa_handler = handle_chld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL)) {
      HANDLE_ERRNO
      exit(-1);
    }
    setenv("!", std::to_string(pid).c_str(), true);
  }
  // wait for non-backgrounded processes to complete before moving on
  if (!_background) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      setenv("?", std::to_string(WEXITSTATUS(status)).c_str(), true);
    }
    setenv("_", _simpleCommands.back()->_arguments.back()->c_str(), true);
  }
  // Clear to prepare for next command
  clear();
}

SimpleCommand * Command::_currentSimpleCommand;

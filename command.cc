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

#include <cstdio>
#include <cstdlib>

#include <iostream>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.hh"
#include "shell.hh"


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

void Command::execute() {
    // Don't do anything if there are no simple commands
    if (_simpleCommands.size() == 0) {
        Shell::prompt();
        return;
    }

    // Print contents of Command data structure
    // print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
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

      // convert to const* char*
      // special thanks to https://stackoverflow.com/questions/48727690/invalid-conversion-from-const-char-to-char-const
      std::vector<char*> argv;
      for (auto arg : cmd->_arguments) argv.push_back(arg->data());
      argv.push_back(NULL);

      // file redirection
      if (cmd == _simpleCommands.front()) { // first command, open input/error redirect file if necessary
        if (_inFile) ifd = open(_inFile->c_str(), O_RDONLY, 0666);
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
        dup2(ifd, STDOUT_FILENO);
        // stderr to specified fd
        dup2(efd, STDERR_FILENO);

        // if current command is not the last one, direct output to next segment
        if (cmd != _simpleCommands.back())
          dup2(pipefd[1], STDOUT_FILENO);
        else // otherwise (last command) send output to specified fd
          dup2(ofd, STDOUT_FILENO);

        close(pipefd[0]); // close input

        // TODO: is exit() necessary here?
        exit(execvp(argv[0], argv.data()));
      } else { // parent
        // wait(NULL); // wait for child to finish before moving on
        close(pipefd[1]); // close segment output
        ifd = pipefd[0];
      }
    }
    // restore stdin, stdout, stderr
    dup2(stdinfd, STDIN_FILENO);
    dup2(stdoutfd, STDOUT_FILENO);
    dup2(stderrfd, STDERR_FILENO);

#if 0
    for (auto cmd : _simpleCommands) {
      pid = fork();

      if (pid == -1) {
        perror("fatal: fork\n");
        exit(2);
      }

      // only need to open output fd once if out and err go to same place
      if ((_outFile || _errFile) && _outFile == _errFile) ofd = efd = creat(_outFile->c_str(), 0666);
      else if (_outFile) ofd = creat(_outFile->c_str(), 0666);
      else if (_errFile) efd = creat(_errFile->c_str(), 0666);

      if (_inFile) ifd = creat(_inFile->c_str(), 0666);

      // TODO: ????
      if (ifd < 0) {
        perror("fatal: creat input file\n");
        exit(2);
      }
      if (int e0 = dup2(stdinfd, ifd); e0 == -1) {
        perror("fatal: redirect stdin\n");
        exit(2);
      }
      // end TODO
      if (ofd < 0) {
        perror("fatal: creat output file\n");
        exit(2);
      }
      if (int e1 = dup2(ofd, 1); e1 == -1) {
        perror("fatal: redirect stdout\n");
        exit(2);
      }
      if (efd < 0) {
        perror("fatal: creat error file\n");
        exit(2);
      }
      if (int e2 = dup2(efd, 2); e2 == -1) {
        perror("fatal: redirect stderr\n");
        exit(2);
      }


      int fdpipe[2];
      if (_simpleCommands.size() > 1) {
        if (pipe(fdpipe) == -1) {
          perror("fatal: pipe\n");
          exit(2);
        }
        dup2(fdpipe[1], 1);
      }

      if (pid == 0) {
        close(stdinfd);
        close(stdoutfd);
        close(stderrfd);

        // special thanks to https://stackoverflow.com/questions/48727690/invalid-conversion-from-const-char-to-char-const
        std::vector<char*> argv;
        for (auto arg : cmd->_arguments) argv.push_back(arg->data());
        argv.push_back(NULL);

        execvp(cmd->_arguments[0]->c_str(), argv.data());
      }
      dup2(stdinfd, 0);
      dup2(stdoutfd, 1);
      dup2(stderrfd, 2);
    }
    // restore stdin, stdout, stderr
    // dup2(0, stdinfd);
    // dup2(2, stderrfd);
#endif

    if (_background) std::cout << "[1] " << pid << std::endl;
    if (!_background) waitpid(pid, NULL, 0);
    // Clear to prepare for next command
    clear();

    // Print new prompt if stdin is a tty
    if (isatty(0)) Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;

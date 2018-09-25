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

#include <sys/types.h>
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
    print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
    int pid;
    for (auto cmd : _simpleCommands) {
      std::cerr << "before fork" << std::endl;
      pid = fork();

      if (pid == -1) {
        perror("fork\n");
        exit(2);
      }

      // int df_in = dup(0);
      // int df_out = dup(1);
      // int df_err = dup(2);

      // TODO: pipe
      // int fdpipe[_simpleCommands.size()];

      std::cerr << pid << std::endl;
      if (pid == 0) {
        // convert from (std::string*) to (const* char*)
        std::vector<char*> args(cmd->_arguments.size());
        for (auto arg : cmd->_arguments) args.push_back(arg->data());
        args.push_back(NULL);
        std::cerr << args[0] << std::endl;
        execvp(args[0], args.data());
      }
    }
    waitpid(pid, NULL, 0);
    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;

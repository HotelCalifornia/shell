
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires
{
#include <algorithm> // std::remove
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT LESS NEWLINE AMP GREATGREAT ERRGREAT GREATAMP ERRGREATGREAT GREATGREATAMP PIPE SOURCE_COMMAND

%{
//#define yylex yylex
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "shell.hh"

void yyerror(const char * s);
int yylex();

%}

%%

goal:
  commands
  ;

commands:
  command_line
  | commands command_line
  ;

command_line:
  pipe_list iomodifier_list background_opt NEWLINE {
    Shell::_currentCommand.execute();
    Shell::prompt();
  }
  | NEWLINE
  | error NEWLINE { yyerrok; }
  ;

pipe_list:
  pipe_list PIPE command_and_args
  | command_and_args
  ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

command_word:
  WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  | SOURCE_COMMAND WORD { /* source command */
    Shell::source($2);
  }
  | SOURCE_COMMAND { /* invalid syntax */
    yyerror("usage: source filename");
    yyerrok;
  }
  ;

argument_list:
  argument_list argument
  | %empty /* can be empty */
  ;

argument:
  WORD {
    /* remove quotes from compound argument */
    /* if ($1->front() == '\"' && $1->back() == '\"') { /* check front and back as a precaution

      *$1 = $1->substr(1, $1->size() - 2);
    } */
    printf("dbg: %s\n", $1->c_str());
    if ($1->front() == '$' && *($1->begin() + 1) == '(' && $1->back() == ')') { /* subshell */
      /* strip '$(' and ')' */
      auto cmd = std::string($1->begin() + 2, $1->end());
      pid_t pid;

      int cmdPipe[2]; pipe(cmdPipe);
      int outPipe[2]; pipe(outPipe);

      if ((pid = fork()) == -1) {
        yyerror("fatal: fork\n");
        exit(-1);
      } else if (pid == 0) { /* child */
        dup2(cmdPipe[0], STDIN_FILENO); close(cmdPipe[1]);
        dup2(outPipe[1], STDOUT_FILENO); close(outPipe[0]);
        exit(execlp("/proc/self/exe", "subshell"));
      } else { /* parent */
        close(cmdPipe[0]);
        close(outPipe[1]);
        dprintf(cmdPipe[1], "%s\n", cmd.c_str());
        /* wait for child proc to finish in order to ensure all the data we want is avail */
        waitpid(pid, NULL, 0);
        std::string subshell_out = "";
        FILE* stream;
        char c;
        stream = fdopen(outPipe[0], "r");
        while ((c = fgetc(stream)) != EOF) {
          if (c == '\n') subshell_out += ' ';
          else subshell_out += c;
        }
        Command::_currentSimpleCommand->insertArgument(&subshell_out);
      }
    } else {
      $1->erase(Shell::remove_if_unescaped($1, '\"'), $1->end());
      /* handle escaped characters */
      $1->erase(Shell::remove_if_unescaped($1, '\\'), $1->end());
      /* run it again to clean up leftovers */
      $1->erase(Shell::remove_if_unescaped($1, '\\'), $1->end());

      Command::_currentSimpleCommand->insertArgument($1);
    }
  }
  ;

iomodifier_list:
  iomodifier_list iomodifier_opt
  | %empty
  ;

iomodifier_opt:
  GREAT WORD { /* standard output redirection */
    if (Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._outFile = $2;
  }
  | LESS WORD { /* standard input redirection */
    if (Shell::_currentCommand._inFile) {
      yyerror("Ambiguous input redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._inFile = $2;
  }
  | GREATGREAT WORD { /* redirect stdout and append */
    if (Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._s_append = true;
    Shell::_currentCommand._outFile = $2;
  }
  | ERRGREAT WORD { /* redirect only stderr */
    if (Shell::_currentCommand._errFile) {
      yyerror("Ambiguous error redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._errFile = $2;
  }
  | ERRGREATGREAT WORD { /* redirect only stderr and append */
    if (Shell::_currentCommand._errFile) {
      yyerror("Ambiguous error redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._e_append = true;
    Shell::_currentCommand._errFile = $2;
  }
  | GREATAMP WORD { /* redirect stdout and stderr */
    if (Shell::_currentCommand._outFile || Shell::_currentCommand._errFile) {
      yyerror("Ambiguous redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._outFile = Shell::_currentCommand._errFile = $2;
  }
  | GREATGREATAMP WORD { /* redirect stdout and stderr and append */
    if (Shell::_currentCommand._outFile || Shell::_currentCommand._errFile) {
      yyerror("Ambiguous redirect.\n");
      exit(-1);
    }
    Shell::_currentCommand._s_append = Shell::_currentCommand._e_append = true;
    Shell::_currentCommand._outFile = Shell::_currentCommand._errFile = $2;
  }
  ;

background_opt:
  AMP {
    Shell::_currentCommand._background = true;
  }
  | %empty /* empty */
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif

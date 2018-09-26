
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
%token NOTOKEN EXIT GREAT LESS NEWLINE AMP GREATGREAT ERRGREAT GREATAMP ERRGREATGREAT GREATGREATAMP PIPE

%{
//#define yylex yylex
#include <cstdio>
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
  ;

argument_list:
  argument_list argument
  | %empty /* can be empty */
  ;

argument:
  WORD {
    Command::_currentSimpleCommand->insertArgument( $1 );\
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

logout:
  EXIT {
    fprintf(stdout, "logout\n");
    exit(0);
  }
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

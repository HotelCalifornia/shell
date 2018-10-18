
/*
 *
 * CS-252
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cerrno>
#include <cstdio>
#include <cstring>
#include "y.tab.hh"
#include "shell.hh"

static void yyunput(int c, char* buf_ptr);

void myunputc(int c) {
  unput(c);
}

void lsource(std::string* fname) {
  FILE* sfile = fopen(fname->c_str(), "r");
  if (sfile) {
    yypush_buffer_state(yy_create_buffer(sfile, YY_BUF_SIZE));
    yyparse();
    yypop_buffer_state();
    yyrestart(stdin);
  } else {
    perror(strerror(errno));
  }
}

%}

%%

"source" {
  return SOURCE_COMMAND;
}

\$\([^\n]*\) {
  Shell::do_subshell(new std::string(yytext));
}

(([\\]([\\][\\])*[^\n\t]|[^&<>|\n\t \\]|([\\][\\])+)+)|(\"[^\"]*\") {
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

[\n] {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}
">>" {
  return GREATGREAT;
}
"2>" {
  return ERRGREAT;
}
"2>>" {
  return ERRGREATGREAT;
}
">&" {
  return GREATAMP;
}
">>&" {
  return GREATGREATAMP;
}
"<" {
  return LESS;
}

"&" {
  return AMP;
}

"|" {
  return PIPE;
}

<<EOF>> {
  if (isatty(0) && !Shell::is_subshell()) printf("logout\n");
  exit(0);
}

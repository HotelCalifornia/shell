
/*
 *
 * CS-252
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include "y.tab.hh"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

%}

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

"exit" {
  return EXIT;
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

[^ \t\n][^ \t\n]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

\".*\" {
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

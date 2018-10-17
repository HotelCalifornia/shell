#ifndef shell_hh
#define shell_hh

#include <algorithm>
#include <string>

#include "command.hh"

struct Shell {
  static void do_subshell(std::string* input);
  static bool is_subshell();
  static void prompt(bool newline = false);
  static void source(std::string* fname, bool needresume = true);

  static auto remove_if_unescaped(std::string* s, char rem) {
    auto first = std::find(s->begin(), s->end(), rem);
    if (first != s->end()) {
      for (auto i = first; ++i != s->end(); ) {
        /* ignore characters that aren't double quotes */
        if (*i != rem) *first++ = std::move(*i);
        /* ignore escaped double quotes */
        else if (i != s->begin() && *(i - 1) == '\\') *first++ = std::move(*i);
      }
    }
    return first;
  }

  static Command _currentCommand;
};

#endif

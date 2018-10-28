#ifndef command_hh
#define command_hh

#include <optional>
#include <regex>
#include <tuple>
#include "simpleCommand.hh"

// Command Data Structure

struct Command {
private:
  void clear();
  void print();
  void expand();
  void tilde(std::string* arg);
  void env(std::string* arg);

  std::string prep_wildcard_path(std::string path);
  bool has_wildcard_pattern(std::string arg);
  std::tuple<std::string, std::string> get_path_and_file_component(std::string path);
  std::vector<std::string> wildcard_helper(
    std::string dir,
    std::string pattern,
    std::optional<std::vector<std::string>> matches
  );
  std::regex wildcard_pattern_to_regex(std::string pattern);
  bool wildcard_match(std::string name, std::string pattern);
  std::vector<std::string> wildcard(std::string arg);
  void expand_wildcard(std::string* arg);
public:
  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  void execute();

  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  bool _s_append;
  bool _e_append;
  bool _background;
  static SimpleCommand *_currentSimpleCommand;
};

#endif

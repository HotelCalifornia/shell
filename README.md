# Specified Features

## Working

1. Parsing and Executing Commands
  a. complex commands
  b. executing commands
    1. simple command process creation and execution
    2. file redirection
    3. pipes
    4. isatty
2. Signal Handling, More Parsing, and Subshells
  1. ctrl+C
  2. zombie elimination
  3. exit
  4. quotes
  5. escaping
  6. builtin functions
  7. default source file .shellrc
  8. subshells
3. Expansions, Wildcards, and Line Editing
  1. environment variable expansion
  2. tilde expansion
  3. wildcarding
  7. variable prompt

## Not Working

3. Expansions, Wildcards, and Line Editing
  4. edit mode
  5. history

# Extra features

- `cd -` returns to previous directory

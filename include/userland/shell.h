#ifndef _USERLAND_SHELL_H
#define _USERLAND_SHELL_H

#include <stdbool.h>

#define MAX_COMMAND_LENGTH 256

void shell_init(void);
void shell_run(void);

// Shared builtin dispatcher.
//
// Some commands (neofetch, calc, matrix, history, tty, ...) are shell builtins
// rather than LazyBox applets. Both the kernel TTY shell and the desktop
// terminal route through this so the two consoles accept the same vocabulary.
//
// `raw_cmd` is the untokenized line, which the argument-slurping builtins such
// as calc and hexdump need. Returns true if the command was recognised.
//
// `allow_blocking` is false for callers that must not be suspended -- the
// desktop compositor drives input from its own frame loop, so builtins that
// animate or wait for a key are refused there.
bool shell_execute_builtin(const char* raw_cmd, int argc, char** argv, bool allow_blocking);

// True when the name is a builtin, regardless of whether it can run here.
bool shell_is_builtin(const char* name);
bool shell_builtin_blocks(const char* name);

#endif // _USERLAND_SHELL_H

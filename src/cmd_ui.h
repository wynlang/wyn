#ifndef WYN_CMD_UI_H
#define WYN_CMD_UI_H

// wyn ui - interactive command browser (full-screen TUI).
// argv is main()'s argv (argv[1] is "ui" or "tui"); version is the wyn
// version string for the header bar. Returns a process exit code.
// `wyn ui --list-commands` prints the command table (one name per line)
// on every platform - the coverage test diffs it against main.c's dispatch.
int cmd_ui(int argc, char** argv, const char* version);

// ── the command table as the single source of truth for ACCEPTED FLAGS (V-13) ──
//
// src/cmd_ui.c's CMDS[] already carries, per command, the flags it accepts and its
// positional choices. main.c reads them through these four functions so that an
// unknown flag can be REFUSED instead of silently ignored, without a second flag
// table existing anywhere. `sub` is the subcommand word for two-word commands
// ("pkg" + "add"); pass NULL when there is none.
#include <stddef.h>
int  wyn_cli_command_known(const char* command, const char* sub);
int  wyn_cli_accepts_flag(const char* command, const char* sub, const char* flag,
                          int* takes_value);
void wyn_cli_flag_list(const char* command, const char* sub, char* out, size_t out_sz);
const char* wyn_cli_command_choices(const char* command, const char* sub);

#endif

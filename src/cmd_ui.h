#ifndef WYN_CMD_UI_H
#define WYN_CMD_UI_H

// wyn ui - interactive command browser (full-screen TUI).
// argv is main()'s argv (argv[1] is "ui" or "tui"); version is the wyn
// version string for the header bar. Returns a process exit code.
// `wyn ui --list-commands` prints the command table (one name per line)
// on every platform - the coverage test diffs it against main.c's dispatch.
int cmd_ui(int argc, char** argv, const char* version);

#endif

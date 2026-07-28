#ifndef WYN_COMMANDS_H
#define WYN_COMMANDS_H

// Command implementations
int cmd_compile(const char* target, int argc, char** argv);
int cmd_test(const char* test_dir, int argc, char** argv);
int cmd_fmt(const char* file, int argc, char** argv);
// wyn fix: migrate removed syntax (&&->and, ||->or, elseif->else if, !->not).
// Returns >0 = substitutions made, 0 = clean, -1 = error. Counts unfixable |>.
int cmd_fix_file(const char* file, int check_only, int* out_pipe_warns);
int cmd_repl(int argc, char** argv);
int cmd_doc(const char* file, int argc, char** argv);
int cmd_lsp(int argc, char** argv);
int cmd_debug(const char* program, int argc, char** argv);
int cmd_init(const char* name, int argc, char** argv);
int cmd_watch(const char* file, int argc, char** argv);
int cmd_version(int argc, char** argv);
int cmd_help(const char* command, int argc, char** argv);

// Portable recursive directory creation (a la `mkdir -p`). Accepts a
// '/'-separated (or '\'-separated on Windows) path, creates every component,
// tolerates already-existing directories. Returns 0 on success, -1 on failure.
// Defined in main.c; safe to call from any command module.
int wyn_mkdir_p(const char* path);

#endif

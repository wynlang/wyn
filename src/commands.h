#ifndef WYN_COMMANDS_H
#define WYN_COMMANDS_H

// Command implementations
int cmd_compile(const char* target, int argc, char** argv);
int cmd_test(const char* test_dir, int argc, char** argv);
int cmd_fmt(const char* file, int argc, char** argv);
int cmd_repl(int argc, char** argv);
int cmd_doc(const char* file, int argc, char** argv);
int cmd_pkg(int argc, char** argv);
int cmd_lsp(int argc, char** argv);
int cmd_debug(const char* program, int argc, char** argv);
int cmd_init(const char* name, int argc, char** argv);
int cmd_version(int argc, char** argv);
int cmd_help(const char* command, int argc, char** argv);

#endif

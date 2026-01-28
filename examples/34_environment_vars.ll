; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %all_env = alloca i32, align 4
  %port = alloca i32, align 4
  %config = alloca i32, align 4
  %shell = alloca i32, align 4
  %user = alloca i32, align 4
  %home = alloca i32, align 4
  ret i32 0
}

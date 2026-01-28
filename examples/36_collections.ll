; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %s_empty = alloca i32, align 4
  %sval3 = alloca i32, align 4
  %sval2 = alloca i32, align 4
  %sval1 = alloca i32, align 4
  %top = alloca i32, align 4
  %s_len = alloca i32, align 4
  %s = alloca i32, align 4
  %q_empty = alloca i32, align 4
  %val3 = alloca i32, align 4
  %val2 = alloca i32, align 4
  %val1 = alloca i32, align 4
  %front = alloca i32, align 4
  %q_len = alloca i32, align 4
  %q = alloca i32, align 4
  ret i32 0
}

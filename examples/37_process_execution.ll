; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %files = alloca i32, align 4
  %arg_count = alloca i32, align 4
  %args = alloca i32, align 4
  %code2 = alloca i32, align 4
  %code1 = alloca i32, align 4
  %output = alloca i32, align 4
  ret i32 0
}

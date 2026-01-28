; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [34 x i8] c"Hello, Wyn!\\nThis is a test file.\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %size = alloca i32, align 4
  %copied = alloca i32, align 4
  %content = alloca i32, align 4
  store ptr @str, ptr %content, align 8
  ret i32 0
}

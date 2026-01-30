; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 42)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, ptr @str)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i1 true)
  ret i32 0
}

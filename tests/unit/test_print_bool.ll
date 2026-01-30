; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %b = alloca i1, align 1
  store i1 true, ptr %b, align 1
  %b1 = load i1, ptr %b, align 1
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i1 %b1)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i1 false)
  ret i32 0
}

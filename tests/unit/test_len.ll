; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.1 = private unnamed_addr constant [20 x i8] c"Length of 'Hello': \00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %length = alloca i64, align 8
  %str = alloca ptr, align 8
  store ptr @str, ptr %str, align 8
  %str1 = load ptr, ptr %str, align 8
  %strlen = call i64 @strlen(ptr %str1)
  store i64 %strlen, ptr %length, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.1)
  %length2 = load i64, ptr %length, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.2, i64 %length2)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  ret i32 0
}

declare i64 @strlen(ptr)

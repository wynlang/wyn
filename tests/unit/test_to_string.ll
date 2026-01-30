; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str = private unnamed_addr constant [8 x i8] c"Number:\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [8 x i8] c"String:\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %str = alloca ptr, align 8
  %num = alloca i32, align 4
  store i32 42, ptr %num, align 4
  %num1 = load i32, ptr %num, align 4
  %buffer = call ptr @malloc(i64 32)
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt, i32 %num1)
  store ptr %buffer, ptr %str, align 8
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, ptr @str)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  %num2 = load i32, ptr %num, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 %num2)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %str3 = load ptr, ptr %str, align 8
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr %str3)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.8)
  ret i32 0
}

declare i32 @sprintf(ptr, ptr, ...)

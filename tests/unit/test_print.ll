; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.1 = private unnamed_addr constant [2 x i8] c" \00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.5 = private unnamed_addr constant [2 x i8] c"!\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [16 x i8] c"The answer is: \00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr @str.3)
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %4 = call i32 (ptr, ...) @printf(ptr @nl)
  store i32 42, ptr %x, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %x1 = load i32, ptr %x, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.9, i32 %x1)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.10)
  ret i32 0
}

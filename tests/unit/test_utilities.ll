; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [29 x i8] c"Testing utility functions...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [17 x i8] c"Random numbers: \00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.4 = private unnamed_addr constant [3 x i8] c", \00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.8 = private unnamed_addr constant [22 x i8] c"Sleeping for 100ms...\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [15 x i8] c"Done sleeping!\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.14 = private unnamed_addr constant [26 x i8] c"All utility tests passed!\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %rand = call i32 @rand()
  store i32 %rand, ptr %r1, align 4
  %rand1 = call i32 @rand()
  store i32 %rand1, ptr %r2, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %r12 = load i32, ptr %r1, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %r12)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %r23 = load i32, ptr %r2, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.6, i32 %r23)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr @str.8)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %9 = call i32 @usleep(i32 100000)
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr @str.11)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr @str.14)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.16)
  ret i32 0
}

declare i32 @rand()

declare i32 @usleep(i32)

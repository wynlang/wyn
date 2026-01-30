; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [30 x i8] c"Testing new number methods...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [14 x i8] c"42.min(17) = \00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [14 x i8] c"42.max(17) = \00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.10 = private unnamed_addr constant [19 x i8] c"123.to_string() = \00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.14 = private unnamed_addr constant [29 x i8] c"\E2\9C\93 All new methods working!\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %str = alloca ptr, align 8
  %num = alloca i32, align 4
  %maximum = alloca i32, align 4
  %minimum = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  store i32 42, ptr %a, align 4
  store i32 17, ptr %b, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %cmp = icmp slt i32 %a1, %b2
  %min = select i1 %cmp, i32 %a1, i32 %b2
  store i32 %min, ptr %minimum, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %minimum3 = load i32, ptr %minimum, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %minimum3)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %a4 = load i32, ptr %a, align 4
  %b5 = load i32, ptr %b, align 4
  %cmp6 = icmp sgt i32 %a4, %b5
  %max = select i1 %cmp6, i32 %a4, i32 %b5
  store i32 %max, ptr %maximum, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %maximum7 = load i32, ptr %maximum, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.7, i32 %maximum7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.8)
  store i32 123, ptr %num, align 4
  %num8 = load i32, ptr %num, align 4
  %buffer = call ptr @malloc(i64 32)
  %8 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.9, i32 %num8)
  store ptr %buffer, ptr %str, align 8
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %str9 = load ptr, ptr %str, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr %str9)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr @str.14)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.16)
  ret i32 0
}

declare i32 @sprintf(ptr, ptr, ...)

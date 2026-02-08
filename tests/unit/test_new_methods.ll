; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [23 x i8] c"Testing new methods...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [14 x i8] c"(-42).abs(): \00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [19 x i8] c"\E2\9C\93 abs() working!\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %positive = alloca i32, align 4
  %negative = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  store i32 -42, ptr %negative, align 4
  %negative1 = load i32, ptr %negative, align 4
  %is_neg = icmp slt i32 %negative1, 0
  %neg = sub i32 0, %negative1
  %abs = select i1 %is_neg, i32 %neg, i32 %negative1
  store i32 %abs, ptr %positive, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %positive2 = load i32, ptr %positive, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %positive2)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.7)
  ret i32 0
}

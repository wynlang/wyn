; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [20 x i8] c"Testing time_now...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [8 x i8] c"Time 1:\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.5 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.6 = private unnamed_addr constant [8 x i8] c"Time 2:\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [20 x i8] c"\E2\9C\93 time_now works!\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.14 = private unnamed_addr constant [20 x i8] c"\E2\9C\97 time_now failed\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %t2 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %t1, align 4
  %2 = call i32 @usleep(i32 100000)
  %time_now1 = call i64 @wyn_time_now()
  store i64 %time_now1, ptr %t2, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %t12 = load i64, ptr %t1, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.4, i64 %t12)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.5)
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %t23 = load i64, ptr %t2, align 4
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.9, i64 %t23)
  %10 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %t24 = load i64, ptr %t2, align 4
  %t15 = load i64, ptr %t1, align 4
  %icmp = icmp sgt i64 %t24, %t15
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr @str.11)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.13)
  ret i32 0

if.else:                                          ; preds = %entry
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr @str.14)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.16)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i64 @wyn_time_now()

declare i32 @usleep(i32)

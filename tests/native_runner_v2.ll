; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [229 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [81 x i8] c"\E2\95\91              WYN NATIVE TEST RUNNER (v2.0)                               \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [229 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [43 x i8] c"Using 50 parallel workers (Wyn-controlled)\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [17 x i8] c"Running tests...\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.16 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [43 x i8] c"./scripts/testing/run_tests_parallel.sh 50\00", align 1
@str.20 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [11 x i8] c"Duration: \00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.25 = private unnamed_addr constant [2 x i8] c"s\00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.27 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.28 = private unnamed_addr constant [32 x i8] c"Workers: 50 (Wyn native runner)\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.31 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.33 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.34 = private unnamed_addr constant [21 x i8] c"\E2\9C\85 ALL TESTS PASSED\00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.36 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.37 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.39 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.40 = private unnamed_addr constant [22 x i8] c"\E2\9D\8C SOME TESTS FAILED\00", align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %duration = alloca i64, align 8
  %end = alloca i64, align 8
  %result = alloca i32, align 4
  %start = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.12)
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.15)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr @str.16)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %system = call i32 @system(ptr @str.19)
  store i32 %system, ptr %result, align 4
  %time_now1 = call i64 @wyn_time_now()
  store i64 %time_now1, ptr %end, align 4
  %end2 = load i64, ptr %end, align 4
  %start3 = load i64, ptr %start, align 4
  %sub = sub i64 %end2, %start3
  store i64 %sub, ptr %duration, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.21, ptr @str.20)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %duration4 = load i64, ptr %duration, align 4
  %buffer = call ptr @malloc(i64 32)
  %16 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.24, i64 %duration4)
  %add = add ptr @str.23, %buffer
  %add5 = add ptr %add, @str.25
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr %add5)
  %18 = call i32 (ptr, ...) @printf(ptr @nl.27)
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %result6 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result6, 0
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.32, ptr @str.31)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.33)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.35, ptr @str.34)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.36)
  ret i32 0

if.else:                                          ; preds = %entry
  %25 = call i32 (ptr, ...) @printf(ptr @fmt.38, ptr @str.37)
  %26 = call i32 (ptr, ...) @printf(ptr @nl.39)
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @str.40)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.42)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i64 @wyn_time_now()

declare i32 @system(ptr)

declare i32 @sprintf(ptr, ptr, ...)

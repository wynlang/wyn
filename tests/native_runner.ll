; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [229 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [81 x i8] c"\E2\95\91              WYN NATIVE TEST RUNNER (Phase 1)                            \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [229 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [41 x i8] c"./scripts/testing/parallel_unit_tests.sh\00", align 1
@str.11 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.14 = private unnamed_addr constant [29 x i8] c"Native Wyn runner overhead: \00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.16 = private unnamed_addr constant [2 x i8] c"s\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [40 x i8] c"\E2\9C\93 Wyn APIs working (time_now, system)\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

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
  %system = call i32 @system(ptr @str.10)
  store i32 %system, ptr %result, align 4
  %time_now1 = call i64 @wyn_time_now()
  store i64 %time_now1, ptr %end, align 4
  %end2 = load i64, ptr %end, align 4
  %start3 = load i64, ptr %start, align 4
  %sub = sub i64 %end2, %start3
  store i64 %sub, ptr %duration, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr @str.11)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %duration4 = load i64, ptr %duration, align 4
  %buffer = call ptr @malloc(i64 32)
  %10 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.15, i64 %duration4)
  %add = add ptr @str.14, %buffer
  %add5 = add ptr %add, @str.16
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr %add5)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %result6 = load i32, ptr %result, align 4
  ret i32 %result6
}

declare i64 @wyn_time_now()

declare i32 @system(ptr)

declare i32 @sprintf(ptr, ptr, ...)

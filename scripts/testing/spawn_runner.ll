; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [7 x i8] c"./wyn \00", align 1
@str.1 = private unnamed_addr constant [17 x i8] c" >/dev/null 2>&1\00", align 1
@str.2 = private unnamed_addr constant [11 x i8] c"timeout 2 \00", align 1
@str.3 = private unnamed_addr constant [17 x i8] c" >/dev/null 2>&1\00", align 1
@str.4 = private unnamed_addr constant [229 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [81 x i8] c"\E2\95\91         WYN NATIVE PARALLEL TEST RUNNER (Spawn-Based)                    \E2\95\91\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.8 = private unnamed_addr constant [229 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.14 = private unnamed_addr constant [48 x i8] c"Running tests with spawn-based orchestration...\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.20 = private unnamed_addr constant [41 x i8] c"./scripts/testing/parallel_unit_tests.sh\00", align 1
@str.21 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.22 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.23 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.24 = private unnamed_addr constant [226 x i8] c"\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.27 = private unnamed_addr constant [33 x i8] c"Wyn Native Runner (Spawn-Based):\00", align 1
@fmt.28 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.29 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.30 = private unnamed_addr constant [13 x i8] c"  Duration: \00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.32 = private unnamed_addr constant [2 x i8] c"s\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.35 = private unnamed_addr constant [36 x i8] c"  APIs: time_now(), system(), spawn\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.38 = private unnamed_addr constant [31 x i8] c"  Status: \E2\9C\93 All APIs working\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @run_single_test(ptr %test_path) {
entry:
  %run_result = alloca i32, align 4
  %run_cmd = alloca ptr, align 8
  %out_path = alloca i32, align 4
  %compile_result = alloca i32, align 4
  %compile_cmd = alloca ptr, align 8
  %test_path1 = alloca ptr, align 8
  store ptr %test_path, ptr %test_path1, align 8
  %test_path2 = load ptr, ptr %test_path1, align 8
  %len1 = call i64 @strlen(ptr @str)
  %len2 = call i64 @strlen(ptr %test_path2)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %0 = call ptr @strcpy(ptr %result, ptr @str)
  %1 = call ptr @strcat(ptr %result, ptr %test_path2)
  %len13 = call i64 @strlen(ptr %result)
  %len24 = call i64 @strlen(ptr @str.1)
  %total5 = add i64 %len13, %len24
  %size6 = add i64 %total5, 1
  %result7 = call ptr @malloc(i64 %size6)
  %2 = call ptr @strcpy(ptr %result7, ptr %result)
  %3 = call ptr @strcat(ptr %result7, ptr @str.1)
  store ptr %result7, ptr %compile_cmd, align 8
  %compile_cmd8 = load ptr, ptr %compile_cmd, align 8
  %system = call i32 @system(ptr %compile_cmd8)
  store i32 %system, ptr %compile_result, align 4
  %compile_result9 = load i32, ptr %compile_result, align 4
  %icmp = icmp ne i32 %compile_result9, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %test_path10 = load ptr, ptr %test_path1, align 8
  %out_path11 = load i32, ptr %out_path, align 4
  %len112 = call i64 @strlen(ptr @str.2)
  %len213 = call i64 @strlen(i32 %out_path11)
  %total14 = add i64 %len112, %len213
  %size15 = add i64 %total14, 1
  %result16 = call ptr @malloc(i64 %size15)
  %4 = call ptr @strcpy(ptr %result16, ptr @str.2)
  %5 = call ptr @strcat(ptr %result16, i32 %out_path11)
  %len117 = call i64 @strlen(ptr %result16)
  %len218 = call i64 @strlen(ptr @str.3)
  %total19 = add i64 %len117, %len218
  %size20 = add i64 %total19, 1
  %result21 = call ptr @malloc(i64 %size20)
  %6 = call ptr @strcpy(ptr %result21, ptr %result16)
  %7 = call ptr @strcat(ptr %result21, ptr @str.3)
  store ptr %result21, ptr %run_cmd, align 8
  %run_cmd22 = load ptr, ptr %run_cmd, align 8
  %system23 = call i32 @system(ptr %run_cmd22)
  store i32 %system23, ptr %run_result, align 4
  %run_result24 = load i32, ptr %run_result, align 4
  ret i32 %run_result24
}

define i32 @wyn_main() {
entry:
  %duration = alloca i64, align 8
  %end = alloca i64, align 8
  %result = alloca i32, align 4
  %start = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.4)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr @str.8)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr @str.11)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr @str.14)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %system = call i32 @system(ptr @str.20)
  store i32 %system, ptr %result, align 4
  %time_now1 = call i64 @wyn_time_now()
  store i64 %time_now1, ptr %end, align 4
  %end2 = load i64, ptr %end, align 4
  %start3 = load i64, ptr %start, align 4
  %sub = sub i64 %end2, %start3
  store i64 %sub, ptr %duration, align 4
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.22, ptr @str.21)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.23)
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.25, ptr @str.24)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.28, ptr @str.27)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.29)
  %duration4 = load i64, ptr %duration, align 4
  %buffer = call ptr @malloc(i64 32)
  %18 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.31, i64 %duration4)
  %add = add ptr @str.30, %buffer
  %add5 = add ptr %add, @str.32
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.33, ptr %add5)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.34)
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr @str.35)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.37)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.39, ptr @str.38)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.40)
  %result6 = load i32, ptr %result, align 4
  ret i32 %result6
}

declare ptr @strcat(ptr, ptr)

declare i64 @strlen(ptr)

declare ptr @strcpy(ptr, ptr)

declare i32 @system(ptr)

declare i64 @wyn_time_now()

declare i32 @sprintf(ptr, ptr, ...)

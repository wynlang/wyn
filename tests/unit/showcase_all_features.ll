; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [46 x i8] c"\E2\95\91   WYN LANGUAGE - FEATURE SHOWCASE     \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [21 x i8] c"1. Boolean Literals:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [24 x i8] c"   \E2\9C\93 System is active\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.16 = private unnamed_addr constant [24 x i8] c"   \E2\9C\97 Should not print\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [25 x i8] c"   \E2\9C\93 System is enabled\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.22 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.23 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.24 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.25 = private unnamed_addr constant [19 x i8] c"2. Math Functions:\00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.27 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.28 = private unnamed_addr constant [18 x i8] c"   min(42, 17) = \00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.30 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.31 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.32 = private unnamed_addr constant [18 x i8] c"   max(42, 17) = \00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.34 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.35 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.36 = private unnamed_addr constant [15 x i8] c"   abs(-99) = \00", align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.39 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.40 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.43 = private unnamed_addr constant [20 x i8] c"3. Print Functions:\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.46 = private unnamed_addr constant [18 x i8] c"   print() works \00", align 1
@fmt.47 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.48 = private unnamed_addr constant [9 x i8] c"without \00", align 1
@fmt.49 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.50 = private unnamed_addr constant [10 x i8] c"newlines!\00", align 1
@fmt.51 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.52 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.53 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.54 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.55 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.56 = private unnamed_addr constant [17 x i8] c"4. Option Types:\00", align 1
@fmt.57 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.58 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.59 = private unnamed_addr constant [28 x i8] c"   \E2\9C\93 Option types working\00", align 1
@fmt.60 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.61 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.62 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.63 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.64 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.65 = private unnamed_addr constant [17 x i8] c"5. Result Types:\00", align 1
@fmt.66 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.67 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.68 = private unnamed_addr constant [28 x i8] c"   \E2\9C\93 Result types working\00", align 1
@fmt.69 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.70 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.71 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.72 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.73 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.74 = private unnamed_addr constant [13 x i8] c"6. File I/O:\00", align 1
@fmt.75 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.76 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.77 = private unnamed_addr constant [22 x i8] c"/tmp/wyn_showcase.txt\00", align 1
@str.78 = private unnamed_addr constant [16 x i8] c"Wyn is awesome!\00", align 1
@str.79 = private unnamed_addr constant [33 x i8] c"   \E2\9C\93 File written successfully\00", align 1
@fmt.80 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.81 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.82 = private unnamed_addr constant [22 x i8] c"/tmp/wyn_showcase.txt\00", align 1
@str.83 = private unnamed_addr constant [19 x i8] c"   \E2\9C\93 File exists\00", align 1
@fmt.84 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.85 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.86 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.87 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.88 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.89 = private unnamed_addr constant [22 x i8] c"7. Combined Features:\00", align 1
@fmt.90 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.91 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.92 = private unnamed_addr constant [37 x i8] c"   \E2\9C\93 Booleans + Math work together\00", align 1
@fmt.93 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.94 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.95 = private unnamed_addr constant [36 x i8] c"   \E2\9C\93 Options + Math work together\00", align 1
@fmt.96 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.97 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.98 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.99 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.100 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.101 = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt.102 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.103 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.104 = private unnamed_addr constant [48 x i8] c"\E2\95\91  ALL FEATURES WORKING PERFECTLY! \E2\9C\93    \E2\95\91\00", align 1
@fmt.105 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.106 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.107 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.108 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.109 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %opt_result = alloca ptr, align 8
  %result = alloca i32, align 4
  %flag = alloca i1, align 1
  %exists = alloca i32, align 4
  %write_ok = alloca i32, align 4
  %failure = alloca ptr, align 8
  %success = alloca ptr, align 8
  %empty = alloca ptr, align 8
  %value = alloca ptr, align 8
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %is_disabled = alloca i1, align 1
  %is_active = alloca i1, align 1
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.12)
  store i1 true, ptr %is_active, align 1
  store i1 false, ptr %is_disabled, align 1
  %is_active1 = load i1, ptr %is_active, align 1
  br i1 %is_active1, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.15)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %is_disabled2 = load i1, ptr %is_disabled, align 1
  br i1 %is_disabled2, label %if.then3, label %if.else

if.then3:                                         ; preds = %if.end
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr @str.16)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.18)
  br label %if.end4

if.else:                                          ; preds = %if.end
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.21)
  br label %if.end4

if.end4:                                          ; preds = %if.else, %if.then3
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.23, ptr @str.22)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.24)
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr @str.25)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.27)
  store i32 42, ptr %a, align 4
  store i32 17, ptr %b, align 4
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %a5 = load i32, ptr %a, align 4
  %b6 = load i32, ptr %b, align 4
  %wyn_min = call i32 @wyn_min(i32 %a5, i32 %b6)
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.30, i32 %wyn_min)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.31)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.33, ptr @str.32)
  %a7 = load i32, ptr %a, align 4
  %b8 = load i32, ptr %b, align 4
  %wyn_max = call i32 @wyn_max(i32 %a7, i32 %b8)
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.34, i32 %wyn_max)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.35)
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.37, ptr @str.36)
  %wyn_abs = call i32 @wyn_abs(i32 -99)
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.38, i32 %wyn_abs)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.39)
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @str.40)
  %30 = call i32 (ptr, ...) @printf(ptr @nl.42)
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.47, ptr @str.46)
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.49, ptr @str.48)
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.51, ptr @str.50)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.52)
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.54, ptr @str.53)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.55)
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.57, ptr @str.56)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.58)
  %tmp = alloca i32, align 4
  store i32 100, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %value, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %empty, align 8
  %41 = call i32 (ptr, ...) @printf(ptr @fmt.60, ptr @str.59)
  %42 = call i32 (ptr, ...) @printf(ptr @nl.61)
  %43 = call i32 (ptr, ...) @printf(ptr @fmt.63, ptr @str.62)
  %44 = call i32 (ptr, ...) @printf(ptr @nl.64)
  %45 = call i32 (ptr, ...) @printf(ptr @fmt.66, ptr @str.65)
  %46 = call i32 (ptr, ...) @printf(ptr @nl.67)
  %tmp9 = alloca i32, align 4
  store i32 200, ptr %tmp9, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp9, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %success, align 8
  %tmp10 = alloca i32, align 4
  store i32 404, ptr %tmp10, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp10, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_err, ptr %failure, align 8
  %47 = call i32 (ptr, ...) @printf(ptr @fmt.69, ptr @str.68)
  %48 = call i32 (ptr, ...) @printf(ptr @nl.70)
  %49 = call i32 (ptr, ...) @printf(ptr @fmt.72, ptr @str.71)
  %50 = call i32 (ptr, ...) @printf(ptr @nl.73)
  %51 = call i32 (ptr, ...) @printf(ptr @fmt.75, ptr @str.74)
  %52 = call i32 (ptr, ...) @printf(ptr @nl.76)
  %wyn_file_write_simple = call i32 @wyn_file_write_simple(ptr @str.77, ptr @str.78)
  store i32 %wyn_file_write_simple, ptr %write_ok, align 4
  %write_ok11 = load i32, ptr %write_ok, align 4
  %icmp = icmp eq i32 %write_ok11, 1
  br i1 %icmp, label %if.then12, label %if.end13

if.then12:                                        ; preds = %if.end4
  %53 = call i32 (ptr, ...) @printf(ptr @fmt.80, ptr @str.79)
  %54 = call i32 (ptr, ...) @printf(ptr @nl.81)
  br label %if.end13

if.end13:                                         ; preds = %if.then12, %if.end4
  %wyn_file_exists_simple = call i32 @wyn_file_exists_simple(ptr @str.82)
  store i32 %wyn_file_exists_simple, ptr %exists, align 4
  %exists14 = load i32, ptr %exists, align 4
  %icmp15 = icmp eq i32 %exists14, 1
  br i1 %icmp15, label %if.then16, label %if.end17

if.then16:                                        ; preds = %if.end13
  %55 = call i32 (ptr, ...) @printf(ptr @fmt.84, ptr @str.83)
  %56 = call i32 (ptr, ...) @printf(ptr @nl.85)
  br label %if.end17

if.end17:                                         ; preds = %if.then16, %if.end13
  %57 = call i32 (ptr, ...) @printf(ptr @fmt.87, ptr @str.86)
  %58 = call i32 (ptr, ...) @printf(ptr @nl.88)
  %59 = call i32 (ptr, ...) @printf(ptr @fmt.90, ptr @str.89)
  %60 = call i32 (ptr, ...) @printf(ptr @nl.91)
  store i1 true, ptr %flag, align 1
  %wyn_min18 = call i32 @wyn_min(i32 10, i32 20)
  store i32 %wyn_min18, ptr %result, align 4
  %flag19 = load i1, ptr %flag, align 1
  br i1 %flag19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end17
  %result22 = load i32, ptr %result, align 4
  %icmp23 = icmp eq i32 %result22, 10
  br i1 %icmp23, label %if.then24, label %if.end25

if.end21:                                         ; preds = %if.end25, %if.end17
  %wyn_max26 = call i32 @wyn_max(i32 5, i32 15)
  %tmp27 = alloca i32, align 4
  store i32 %wyn_max26, ptr %tmp27, align 4
  %wyn_some28 = call ptr @wyn_some(ptr %tmp27, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some28, ptr %opt_result, align 8
  %61 = call i32 (ptr, ...) @printf(ptr @fmt.96, ptr @str.95)
  %62 = call i32 (ptr, ...) @printf(ptr @nl.97)
  %63 = call i32 (ptr, ...) @printf(ptr @fmt.99, ptr @str.98)
  %64 = call i32 (ptr, ...) @printf(ptr @nl.100)
  %65 = call i32 (ptr, ...) @printf(ptr @fmt.102, ptr @str.101)
  %66 = call i32 (ptr, ...) @printf(ptr @nl.103)
  %67 = call i32 (ptr, ...) @printf(ptr @fmt.105, ptr @str.104)
  %68 = call i32 (ptr, ...) @printf(ptr @nl.106)
  %69 = call i32 (ptr, ...) @printf(ptr @fmt.108, ptr @str.107)
  %70 = call i32 (ptr, ...) @printf(ptr @nl.109)
  ret i32 0

if.then24:                                        ; preds = %if.then20
  %71 = call i32 (ptr, ...) @printf(ptr @fmt.93, ptr @str.92)
  %72 = call i32 (ptr, ...) @printf(ptr @nl.94)
  br label %if.end25

if.end25:                                         ; preds = %if.then24, %if.then20
  br label %if.end21
}

declare i32 @wyn_min(i32, i32)

declare i32 @wyn_max(i32, i32)

declare i32 @wyn_abs(i32)

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_none()

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_err(ptr, i64)

declare i32 @wyn_file_write_simple(ptr, ptr)

declare i32 @wyn_file_exists_simple(ptr)

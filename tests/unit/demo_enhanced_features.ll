; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [44 x i8] c"=== Wyn Language Enhanced Features Demo ===\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [20 x i8] c"1. Print Functions:\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [33 x i8] c"   - print() and println() work!\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.9 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [19 x i8] c"2. Math Functions:\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.15 = private unnamed_addr constant [18 x i8] c"   min(15, 25) = \00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [18 x i8] c"   max(15, 25) = \00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [15 x i8] c"   abs(-42) = \00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.27 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.28 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.29 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.30 = private unnamed_addr constant [24 x i8] c"3. Option/Result Types:\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.33 = private unnamed_addr constant [36 x i8] c"   Option and Result types working!\00", align 1
@fmt.34 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.35 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.36 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.38 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.39 = private unnamed_addr constant [13 x i8] c"4. File I/O:\00", align 1
@fmt.40 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.41 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.42 = private unnamed_addr constant [18 x i8] c"/tmp/wyn_demo.txt\00", align 1
@str.43 = private unnamed_addr constant [16 x i8] c"Hello from Wyn!\00", align 1
@str.44 = private unnamed_addr constant [30 x i8] c"   File written successfully!\00", align 1
@fmt.45 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.46 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.47 = private unnamed_addr constant [18 x i8] c"/tmp/wyn_demo.txt\00", align 1
@str.48 = private unnamed_addr constant [16 x i8] c"   File exists!\00", align 1
@fmt.49 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.50 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.51 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.52 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.53 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.54 = private unnamed_addr constant [30 x i8] c"=== All features working! ===\00", align 1
@fmt.55 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.56 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %exists = alloca i32, align 4
  %write_result = alloca i32, align 4
  %failure = alloca ptr, align 8
  %success = alloca ptr, align 8
  %nothing = alloca ptr, align 8
  %opt = alloca ptr, align 8
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr @str.9)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %10 = call i32 (ptr, ...) @printf(ptr @nl.14)
  store i32 15, ptr %a, align 4
  store i32 25, ptr %b, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.16, ptr @str.15)
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %wyn_min = call i32 @wyn_min(i32 %a1, i32 %b2)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, i32 %wyn_min)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %a3 = load i32, ptr %a, align 4
  %b4 = load i32, ptr %b, align 4
  %wyn_max = call i32 @wyn_max(i32 %a3, i32 %b4)
  %15 = call i32 (ptr, ...) @printf(ptr @fmt.21, i32 %wyn_max)
  %16 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %wyn_abs = call i32 @wyn_abs(i32 -42)
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.25, i32 %wyn_abs)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.28, ptr @str.27)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.29)
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr @str.30)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %tmp = alloca i32, align 4
  store i32 100, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %opt, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %nothing, align 8
  %tmp5 = alloca i32, align 4
  store i32 200, ptr %tmp5, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp5, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %success, align 8
  %tmp6 = alloca i32, align 4
  store i32 404, ptr %tmp6, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp6, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_err, ptr %failure, align 8
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.34, ptr @str.33)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.35)
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.37, ptr @str.36)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.38)
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.40, ptr @str.39)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.41)
  %wyn_file_write_simple = call i32 @wyn_file_write_simple(ptr @str.42, ptr @str.43)
  store i32 %wyn_file_write_simple, ptr %write_result, align 4
  %write_result7 = load i32, ptr %write_result, align 4
  %icmp = icmp eq i32 %write_result7, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.45, ptr @str.44)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.46)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %wyn_file_exists_simple = call i32 @wyn_file_exists_simple(ptr @str.47)
  store i32 %wyn_file_exists_simple, ptr %exists, align 4
  %exists8 = load i32, ptr %exists, align 4
  %icmp9 = icmp eq i32 %exists8, 1
  br i1 %icmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.49, ptr @str.48)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.50)
  br label %if.end11

if.end11:                                         ; preds = %if.then10, %if.end
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.52, ptr @str.51)
  %35 = call i32 (ptr, ...) @printf(ptr @nl.53)
  %36 = call i32 (ptr, ...) @printf(ptr @fmt.55, ptr @str.54)
  %37 = call i32 (ptr, ...) @printf(ptr @nl.56)
  ret i32 0
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

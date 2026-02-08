; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [26 x i8] c"=== New Features Test ===\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [21 x i8] c"1. Boolean Literals:\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [26 x i8] c"   \E2\9C\93 true literal works\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [30 x i8] c"   \E2\9C\97 false should not print\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [27 x i8] c"   \E2\9C\93 false literal works\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.16 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [19 x i8] c"2. len() Function:\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.22 = private unnamed_addr constant [13 x i8] c"Wyn Language\00", align 1
@str.23 = private unnamed_addr constant [30 x i8] c"   Length of 'Wyn Language': \00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.27 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.28 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.29 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.30 = private unnamed_addr constant [22 x i8] c"3. typeof() Function:\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.33 = private unnamed_addr constant [5 x i8] c"test\00", align 1
@str.34 = private unnamed_addr constant [17 x i8] c"   typeof(100): \00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@typename = private unnamed_addr constant [4 x i8] c"int\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.38 = private unnamed_addr constant [20 x i8] c"   typeof('test'): \00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@typename.40 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.43 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.46 = private unnamed_addr constant [22 x i8] c"4. Combined Features:\00", align 1
@fmt.47 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.48 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.49 = private unnamed_addr constant [37 x i8] c"   \E2\9C\93 Math + booleans work together\00", align 1
@fmt.50 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.51 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.52 = private unnamed_addr constant [31 x i8] c"   \E2\9C\93 Option types still work\00", align 1
@fmt.53 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.54 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.55 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.56 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.57 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.58 = private unnamed_addr constant [34 x i8] c"=== All new features working! ===\00", align 1
@fmt.59 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.60 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %opt = alloca ptr, align 8
  %result = alloca i32, align 4
  %str = alloca ptr, align 8
  %num = alloca i32, align 4
  %text_len = alloca i64, align 8
  %text = alloca ptr, align 8
  %is_false = alloca i1, align 1
  %is_true = alloca i1, align 1
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  store i1 true, ptr %is_true, align 1
  store i1 false, ptr %is_false, align 1
  %is_true1 = load i1, ptr %is_true, align 1
  br i1 %is_true1, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %is_false2 = load i1, ptr %is_false, align 1
  br i1 %is_false2, label %if.then3, label %if.else

if.then3:                                         ; preds = %if.end
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.12)
  br label %if.end4

if.else:                                          ; preds = %if.end
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.15)
  br label %if.end4

if.end4:                                          ; preds = %if.else, %if.then3
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr @str.16)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.21)
  store ptr @str.22, ptr %text, align 8
  %text5 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text5)
  store i64 %strlen, ptr %text_len, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %text_len6 = load i64, ptr %text_len, align 4
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.25, i64 %text_len6)
  %18 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.28, ptr @str.27)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.29)
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr @str.30)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.32)
  store i32 100, ptr %num, align 4
  store ptr @str.33, ptr %str, align 8
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.35, ptr @str.34)
  %num7 = load i32, ptr %num, align 4
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr @typename)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.37)
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.39, ptr @str.38)
  %str8 = load ptr, ptr %str, align 8
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @typename.40)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.42)
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %30 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.47, ptr @str.46)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.48)
  %wyn_min = call i32 @wyn_min(i32 10, i32 20)
  store i32 %wyn_min, ptr %result, align 4
  %result9 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result9, 10
  br i1 %icmp, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end4
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.50, ptr @str.49)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.51)
  br label %if.end11

if.end11:                                         ; preds = %if.then10, %if.end4
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %opt, align 8
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.53, ptr @str.52)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.54)
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.56, ptr @str.55)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.57)
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.59, ptr @str.58)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.60)
  ret i32 0
}

declare i64 @strlen(ptr)

declare i32 @wyn_min(i32, i32)

declare ptr @wyn_some(ptr, i64)

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [46 x i8] c"\E2\95\91   WYN - ENHANCED OOP METHODS          \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [19 x i8] c"1. String Methods:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.14 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.15 = private unnamed_addr constant [31 x i8] c"   text.contains(\\\22World\\\22) = \00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.22 = private unnamed_addr constant [19 x i8] c"2. Number Methods:\00", align 1
@fmt.23 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.24 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.25 = private unnamed_addr constant [19 x i8] c"   (-100).abs() = \00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.29 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.30 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.31 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.32 = private unnamed_addr constant [23 x i8] c"3. Practical Examples:\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.35 = private unnamed_addr constant [17 x i8] c"user@example.com\00", align 1
@str.36 = private unnamed_addr constant [2 x i8] c"@\00", align 1
@str.37 = private unnamed_addr constant [26 x i8] c"   \E2\9C\93 Valid email format\00", align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.39 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.40 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.43 = private unnamed_addr constant [27 x i8] c"4. Clean, Expressive Code:\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.46 = private unnamed_addr constant [27 x i8] c"   text.contains(\\\22word\\\22)\00", align 1
@fmt.47 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.48 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.49 = private unnamed_addr constant [16 x i8] c"   number.abs()\00", align 1
@fmt.50 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.51 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.52 = private unnamed_addr constant [28 x i8] c"   Everything is an object!\00", align 1
@fmt.53 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.54 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.55 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.56 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.57 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.58 = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt.59 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.60 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.61 = private unnamed_addr constant [48 x i8] c"\E2\95\91   ENHANCED OOP METHODS WORKING! \E2\9C\93     \E2\95\91\00", align 1
@fmt.62 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.63 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.64 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.65 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.66 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %email = alloca ptr, align 8
  %pos = alloca i32, align 4
  %neg = alloca i32, align 4
  %found = alloca i32, align 4
  %text = alloca ptr, align 8
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
  store ptr @str.13, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.14)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %found, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.16, ptr @str.15)
  %found2 = load i32, ptr %found, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.17, i32 %found2)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %15 = call i32 (ptr, ...) @printf(ptr @fmt.23, ptr @str.22)
  %16 = call i32 (ptr, ...) @printf(ptr @nl.24)
  store i32 -100, ptr %neg, align 4
  %neg3 = load i32, ptr %neg, align 4
  %is_neg = icmp slt i32 %neg3, 0
  %neg4 = sub i32 0, %neg3
  %abs = select i1 %is_neg, i32 %neg4, i32 %neg3
  store i32 %abs, ptr %pos, align 4
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr @str.25)
  %pos5 = load i32, ptr %pos, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.27, i32 %pos5)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.28)
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.30, ptr @str.29)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.31)
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.33, ptr @str.32)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.34)
  store ptr @str.35, ptr %email, align 8
  %email6 = load ptr, ptr %email, align 8
  %strstr_result7 = call ptr @strstr(ptr %email6, ptr @str.36)
  %is_found8 = icmp ne ptr %strstr_result7, null
  %contains9 = zext i1 %is_found8 to i32
  %tobool = icmp ne i32 %contains9, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.38, ptr @str.37)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.39)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @str.40)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.42)
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.47, ptr @str.46)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.48)
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.50, ptr @str.49)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.51)
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.53, ptr @str.52)
  %35 = call i32 (ptr, ...) @printf(ptr @nl.54)
  %36 = call i32 (ptr, ...) @printf(ptr @fmt.56, ptr @str.55)
  %37 = call i32 (ptr, ...) @printf(ptr @nl.57)
  %38 = call i32 (ptr, ...) @printf(ptr @fmt.59, ptr @str.58)
  %39 = call i32 (ptr, ...) @printf(ptr @nl.60)
  %40 = call i32 (ptr, ...) @printf(ptr @fmt.62, ptr @str.61)
  %41 = call i32 (ptr, ...) @printf(ptr @nl.63)
  %42 = call i32 (ptr, ...) @printf(ptr @fmt.65, ptr @str.64)
  %43 = call i32 (ptr, ...) @printf(ptr @nl.66)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

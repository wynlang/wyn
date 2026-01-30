; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [46 x i8] c"\E2\95\91   WYN - ENHANCED NUMBER METHODS       \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [19 x i8] c"1. Absolute Value:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [18 x i8] c"   (-42).abs() = \00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.20 = private unnamed_addr constant [20 x i8] c"2. Min/Max Methods:\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [18 x i8] c"   100.min(50) = \00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.27 = private unnamed_addr constant [18 x i8] c"   100.max(50) = \00", align 1
@fmt.28 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.31 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.33 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.34 = private unnamed_addr constant [23 x i8] c"3. Practical Examples:\00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.36 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.37 = private unnamed_addr constant [26 x i8] c"   Clamp 150 to max 100: \00", align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.41 = private unnamed_addr constant [14 x i8] c"   Distance: \00", align 1
@fmt.42 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.43 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.44 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.45 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.46 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.47 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.48 = private unnamed_addr constant [29 x i8] c"4. Pure LLVM Implementation:\00", align 1
@fmt.49 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.50 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.51 = private unnamed_addr constant [34 x i8] c"   All methods compile to LLVM IR\00", align 1
@fmt.52 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.53 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.54 = private unnamed_addr constant [23 x i8] c"   No C function calls\00", align 1
@fmt.55 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.56 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.57 = private unnamed_addr constant [18 x i8] c"   Zero overhead!\00", align 1
@fmt.58 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.59 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.60 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.61 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.62 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.63 = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt.64 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.65 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.66 = private unnamed_addr constant [48 x i8] c"\E2\95\91   NUMBER METHODS WORKING! \E2\9C\93           \E2\95\91\00", align 1
@fmt.67 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.68 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.69 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.70 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.71 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %distance = alloca i32, align 4
  %diff = alloca i32, align 4
  %clamped = alloca i32, align 4
  %max_limit = alloca i32, align 4
  %value = alloca i32, align 4
  %maximum = alloca i32, align 4
  %minimum = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %pos = alloca i32, align 4
  %neg = alloca i32, align 4
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
  store i32 -42, ptr %neg, align 4
  %neg1 = load i32, ptr %neg, align 4
  %is_neg = icmp slt i32 %neg1, 0
  %neg2 = sub i32 0, %neg1
  %abs = select i1 %is_neg, i32 %neg2, i32 %neg1
  store i32 %abs, ptr %pos, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %pos3 = load i32, ptr %pos, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.15, i32 %pos3)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %15 = call i32 (ptr, ...) @printf(ptr @fmt.21, ptr @str.20)
  %16 = call i32 (ptr, ...) @printf(ptr @nl.22)
  store i32 100, ptr %x, align 4
  store i32 50, ptr %y, align 4
  %x4 = load i32, ptr %x, align 4
  %y5 = load i32, ptr %y, align 4
  %cmp = icmp slt i32 %x4, %y5
  %min = select i1 %cmp, i32 %x4, i32 %y5
  store i32 %min, ptr %minimum, align 4
  %x6 = load i32, ptr %x, align 4
  %y7 = load i32, ptr %y, align 4
  %cmp8 = icmp sgt i32 %x6, %y7
  %max = select i1 %cmp8, i32 %x6, i32 %y7
  store i32 %max, ptr %maximum, align 4
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %minimum9 = load i32, ptr %minimum, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.25, i32 %minimum9)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.28, ptr @str.27)
  %maximum10 = load i32, ptr %maximum, align 4
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.29, i32 %maximum10)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.32, ptr @str.31)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.33)
  %25 = call i32 (ptr, ...) @printf(ptr @fmt.35, ptr @str.34)
  %26 = call i32 (ptr, ...) @printf(ptr @nl.36)
  store i32 150, ptr %value, align 4
  store i32 100, ptr %max_limit, align 4
  %value11 = load i32, ptr %value, align 4
  %max_limit12 = load i32, ptr %max_limit, align 4
  %cmp13 = icmp slt i32 %value11, %max_limit12
  %min14 = select i1 %cmp13, i32 %value11, i32 %max_limit12
  store i32 %min14, ptr %clamped, align 4
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.38, ptr @str.37)
  %clamped15 = load i32, ptr %clamped, align 4
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.39, i32 %clamped15)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.40)
  store i32 -25, ptr %diff, align 4
  %diff16 = load i32, ptr %diff, align 4
  %is_neg17 = icmp slt i32 %diff16, 0
  %neg18 = sub i32 0, %diff16
  %abs19 = select i1 %is_neg17, i32 %neg18, i32 %diff16
  store i32 %abs19, ptr %distance, align 4
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.42, ptr @str.41)
  %distance20 = load i32, ptr %distance, align 4
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.43, i32 %distance20)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.44)
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.46, ptr @str.45)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.47)
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.49, ptr @str.48)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.50)
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.52, ptr @str.51)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.53)
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.55, ptr @str.54)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.56)
  %41 = call i32 (ptr, ...) @printf(ptr @fmt.58, ptr @str.57)
  %42 = call i32 (ptr, ...) @printf(ptr @nl.59)
  %43 = call i32 (ptr, ...) @printf(ptr @fmt.61, ptr @str.60)
  %44 = call i32 (ptr, ...) @printf(ptr @nl.62)
  %45 = call i32 (ptr, ...) @printf(ptr @fmt.64, ptr @str.63)
  %46 = call i32 (ptr, ...) @printf(ptr @nl.65)
  %47 = call i32 (ptr, ...) @printf(ptr @fmt.67, ptr @str.66)
  %48 = call i32 (ptr, ...) @printf(ptr @nl.68)
  %49 = call i32 (ptr, ...) @printf(ptr @fmt.70, ptr @str.69)
  %50 = call i32 (ptr, ...) @printf(ptr @nl.71)
  ret i32 0
}

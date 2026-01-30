; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str = private unnamed_addr constant [19 x i8] c"Max int as string:\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.5 = private unnamed_addr constant [19 x i8] c"Min int as string:\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [16 x i8] c"Abs of min int:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.15 = private unnamed_addr constant [13 x i8] c"Min(-10, 5):\00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.17 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.20 = private unnamed_addr constant [13 x i8] c"Max(-10, 5):\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.23 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.24 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.25 = private unnamed_addr constant [11 x i8] c"Min(5, 5):\00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.27 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.28 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.29 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.30 = private unnamed_addr constant [11 x i8] c"Max(5, 5):\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %same_max = alloca i32, align 4
  %same_min = alloca i32, align 4
  %max_val = alloca i32, align 4
  %min_val = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %abs_large = alloca i32, align 4
  %neg_large_str = alloca ptr, align 8
  %neg_large = alloca i32, align 4
  %large_str = alloca ptr, align 8
  %large = alloca i32, align 4
  store i32 2147483647, ptr %large, align 4
  %large1 = load i32, ptr %large, align 4
  %buffer = call ptr @malloc(i64 32)
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt, i32 %large1)
  store ptr %buffer, ptr %large_str, align 8
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, ptr @str)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  %large_str2 = load ptr, ptr %large_str, align 8
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr %large_str2)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.3)
  store i32 -2147483647, ptr %neg_large, align 4
  %neg_large3 = load i32, ptr %neg_large, align 4
  %buffer4 = call ptr @malloc(i64 32)
  %5 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer4, ptr @fmt.4, i32 %neg_large3)
  store ptr %buffer4, ptr %neg_large_str, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %neg_large_str5 = load ptr, ptr %neg_large_str, align 8
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr %neg_large_str5)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %neg_large6 = load i32, ptr %neg_large, align 4
  %is_neg = icmp slt i32 %neg_large6, 0
  %neg = sub i32 0, %neg_large6
  %abs = select i1 %is_neg, i32 %neg, i32 %neg_large6
  store i32 %abs, ptr %abs_large, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.12)
  %abs_large7 = load i32, ptr %abs_large, align 4
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.13, i32 %abs_large7)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.14)
  store i32 -10, ptr %a, align 4
  store i32 5, ptr %b, align 4
  %a8 = load i32, ptr %a, align 4
  %b9 = load i32, ptr %b, align 4
  %cmp = icmp slt i32 %a8, %b9
  %min = select i1 %cmp, i32 %a8, i32 %b9
  store i32 %min, ptr %min_val, align 4
  %a10 = load i32, ptr %a, align 4
  %b11 = load i32, ptr %b, align 4
  %cmp12 = icmp sgt i32 %a10, %b11
  %max = select i1 %cmp12, i32 %a10, i32 %b11
  store i32 %max, ptr %max_val, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.16, ptr @str.15)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.17)
  %min_val13 = load i32, ptr %min_val, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.18, i32 %min_val13)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.21, ptr @str.20)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %max_val14 = load i32, ptr %max_val, align 4
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.23, i32 %max_val14)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.24)
  %b15 = load i32, ptr %b, align 4
  %b16 = load i32, ptr %b, align 4
  %cmp17 = icmp slt i32 %b15, %b16
  %min18 = select i1 %cmp17, i32 %b15, i32 %b16
  store i32 %min18, ptr %same_min, align 4
  %b19 = load i32, ptr %b, align 4
  %b20 = load i32, ptr %b, align 4
  %cmp21 = icmp sgt i32 %b19, %b20
  %max22 = select i1 %cmp21, i32 %b19, i32 %b20
  store i32 %max22, ptr %same_max, align 4
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr @str.25)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.27)
  %same_min23 = load i32, ptr %same_min, align 4
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.28, i32 %same_min23)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.29)
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr @str.30)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %same_max24 = load i32, ptr %same_max, align 4
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.33, i32 %same_max24)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.34)
  ret i32 0
}

declare i32 @sprintf(ptr, ptr, ...)

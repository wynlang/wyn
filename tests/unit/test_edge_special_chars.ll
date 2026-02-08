; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [16 x i8] c"Hello\\nWorld\\t!\00", align 1
@str.1 = private unnamed_addr constant [22 x i8] c"Special chars length:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [3 x i8] c"\\n\00", align 1
@str.5 = private unnamed_addr constant [3 x i8] c"\\t\00", align 1
@str.6 = private unnamed_addr constant [18 x i8] c"Contains newline:\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [14 x i8] c"Contains tab:\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.16 = private unnamed_addr constant [154 x i8] c"This is a very long string that should test if the length function properly returns i64 values without truncation issues that were present before the fix\00", align 1
@str.17 = private unnamed_addr constant [20 x i8] c"Long string length:\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.22 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.23 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.24 = private unnamed_addr constant [16 x i8] c"Zero as string:\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.29 = private unnamed_addr constant [20 x i8] c"Negative as string:\00", align 1
@fmt.30 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.31 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.33 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.34 = private unnamed_addr constant [8 x i8] c"Abs(0):\00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.36 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.38 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.39 = private unnamed_addr constant [10 x i8] c"Abs(-42):\00", align 1
@fmt.40 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.41 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.42 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.43 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %abs_neg = alloca i32, align 4
  %abs_zero = alloca i32, align 4
  %neg_str = alloca ptr, align 8
  %zero_str = alloca ptr, align 8
  %negative = alloca i32, align 4
  %zero = alloca i32, align 4
  %len_long = alloca i64, align 8
  %long = alloca ptr, align 8
  %contains_tab = alloca i32, align 4
  %contains_newline = alloca i32, align 4
  %len_special = alloca i64, align 8
  %special = alloca ptr, align 8
  store ptr @str, ptr %special, align 8
  %special1 = load ptr, ptr %special, align 8
  %strlen = call i64 @strlen(ptr %special1)
  store i64 %strlen, ptr %len_special, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.1)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %len_special2 = load i64, ptr %len_special, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i64 %len_special2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %special3 = load ptr, ptr %special, align 8
  %strstr_result = call ptr @strstr(ptr %special3, ptr @str.4)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains_newline, align 4
  %special4 = load ptr, ptr %special, align 8
  %strstr_result5 = call ptr @strstr(ptr %special4, ptr @str.5)
  %is_found6 = icmp ne ptr %strstr_result5, null
  %contains7 = zext i1 %is_found6 to i32
  store i32 %contains7, ptr %contains_tab, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %contains_newline8 = load i32, ptr %contains_newline, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.9, i32 %contains_newline8)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr @str.11)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %contains_tab9 = load i32, ptr %contains_tab, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, i32 %contains_tab9)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.15)
  store ptr @str.16, ptr %long, align 8
  %long10 = load ptr, ptr %long, align 8
  %strlen11 = call i64 @strlen(ptr %long10)
  store i64 %strlen11, ptr %len_long, align 4
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %len_long12 = load i64, ptr %len_long, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, i64 %len_long12)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.21)
  store i32 0, ptr %zero, align 4
  store i32 -42, ptr %negative, align 4
  %zero13 = load i32, ptr %zero, align 4
  %buffer = call ptr @malloc(i64 32)
  %16 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.22, i32 %zero13)
  store ptr %buffer, ptr %zero_str, align 8
  %negative14 = load i32, ptr %negative, align 4
  %buffer15 = call ptr @malloc(i64 32)
  %17 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer15, ptr @fmt.23, i32 %negative14)
  store ptr %buffer15, ptr %neg_str, align 8
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.25, ptr @str.24)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %zero_str16 = load ptr, ptr %zero_str, align 8
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.27, ptr %zero_str16)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.28)
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.30, ptr @str.29)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.31)
  %neg_str17 = load ptr, ptr %neg_str, align 8
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.32, ptr %neg_str17)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.33)
  %zero18 = load i32, ptr %zero, align 4
  %is_neg = icmp slt i32 %zero18, 0
  %neg = sub i32 0, %zero18
  %abs = select i1 %is_neg, i32 %neg, i32 %zero18
  store i32 %abs, ptr %abs_zero, align 4
  %negative19 = load i32, ptr %negative, align 4
  %is_neg20 = icmp slt i32 %negative19, 0
  %neg21 = sub i32 0, %negative19
  %abs22 = select i1 %is_neg20, i32 %neg21, i32 %negative19
  store i32 %abs22, ptr %abs_neg, align 4
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.35, ptr @str.34)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.36)
  %abs_zero23 = load i32, ptr %abs_zero, align 4
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.37, i32 %abs_zero23)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.38)
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.40, ptr @str.39)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.41)
  %abs_neg24 = load i32, ptr %abs_neg, align 4
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.42, i32 %abs_neg24)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.43)
  ret i32 0
}

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

declare i32 @sprintf(ptr, ptr, ...)

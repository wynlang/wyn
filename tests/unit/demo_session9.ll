; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [26 x i8] c"=== WYN COMPILER DEMO ===\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.4 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.5 = private unnamed_addr constant [16 x i8] c"String methods:\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.13 = private unnamed_addr constant [7 x i8] c" World\00", align 1
@str.14 = private unnamed_addr constant [8 x i8] c"Concat:\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@typename = private unnamed_addr constant [7 x i8] c"string\00", align 1
@str.19 = private unnamed_addr constant [8 x i8] c"Length:\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.22 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.23 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.24 = private unnamed_addr constant [6 x i8] c"Type:\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.30 = private unnamed_addr constant [10 x i8] c"Abs(-10):\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.35 = private unnamed_addr constant [14 x i8] c"42 as string:\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.39 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.40 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.41 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.42 = private unnamed_addr constant [4 x i8] c"xyz\00", align 1
@str.43 = private unnamed_addr constant [21 x i8] c"Multiple calls work:\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.46 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.47 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.48 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.49 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.50 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.51 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.52 = private unnamed_addr constant [30 x i8] c"=== ALL FEATURES WORKING! ===\00", align 1
@fmt.53 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.54 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %c3 = alloca i32, align 4
  %c2 = alloca i32, align 4
  %c1 = alloca i32, align 4
  %str_num = alloca ptr, align 8
  %abs_val = alloca i32, align 4
  %neg = alloca i32, align 4
  %n = alloca i32, align 4
  %text_type = alloca ptr, align 8
  %length = alloca i64, align 8
  %concat = alloca ptr, align 8
  %lower8 = alloca ptr, align 8
  %upper6 = alloca ptr, align 8
  %ends = alloca i32, align 4
  %starts = alloca i32, align 4
  %has_hello = alloca i32, align 4
  %text = alloca ptr, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  store ptr @str.1, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.2)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_hello, align 4
  %text2 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.3)
  %strncmp = call i32 @strncmp(ptr %text2, ptr @str.3, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts, align 4
  %text3 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text3)
  %suffix_len = call i64 @strlen(ptr @str.4)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text3, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.4)
  %is_equal4 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal4 to i32
  store i32 %ends_with, ptr %ends, align 4
  %text5 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text5)
  store ptr %upper, ptr %upper6, align 8
  %text7 = load ptr, ptr %text, align 8
  %lower = call ptr @wyn_string_lower(ptr %text7)
  store ptr %lower, ptr %lower8, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %upper9 = load ptr, ptr %upper6, align 8
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr %upper9)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %lower10 = load ptr, ptr %lower8, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr %lower10)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %len1 = call i64 @strlen(ptr @str.12)
  %len2 = call i64 @strlen(ptr @str.13)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %8 = call ptr @strcpy(ptr %result, ptr @str.12)
  %9 = call ptr @strcat(ptr %result, ptr @str.13)
  store ptr %result, ptr %concat, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr @str.14)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %concat11 = load ptr, ptr %concat, align 8
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr %concat11)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %text12 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text12)
  store i64 %strlen, ptr %length, align 4
  %text13 = load ptr, ptr %text, align 8
  store ptr @typename, ptr %text_type, align 8
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %length14 = load i64, ptr %length, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.22, i64 %length14)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.23)
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.25, ptr @str.24)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %text_type15 = load ptr, ptr %text_type, align 8
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.27, ptr %text_type15)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.28)
  store i32 42, ptr %n, align 4
  store i32 -10, ptr %neg, align 4
  %neg16 = load i32, ptr %neg, align 4
  %is_neg = icmp slt i32 %neg16, 0
  %neg17 = sub i32 0, %neg16
  %abs = select i1 %is_neg, i32 %neg17, i32 %neg16
  store i32 %abs, ptr %abs_val, align 4
  %n18 = load i32, ptr %n, align 4
  %buffer = call ptr @malloc(i64 32)
  %22 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.29, i32 %n18)
  store ptr %buffer, ptr %str_num, align 8
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr @str.30)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %abs_val19 = load i32, ptr %abs_val, align 4
  %25 = call i32 (ptr, ...) @printf(ptr @fmt.33, i32 %abs_val19)
  %26 = call i32 (ptr, ...) @printf(ptr @nl.34)
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr @str.35)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.37)
  %str_num20 = load ptr, ptr %str_num, align 8
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.38, ptr %str_num20)
  %30 = call i32 (ptr, ...) @printf(ptr @nl.39)
  %text21 = load ptr, ptr %text, align 8
  %strstr_result22 = call ptr @strstr(ptr %text21, ptr @str.40)
  %is_found23 = icmp ne ptr %strstr_result22, null
  %contains24 = zext i1 %is_found23 to i32
  store i32 %contains24, ptr %c1, align 4
  %text25 = load ptr, ptr %text, align 8
  %strstr_result26 = call ptr @strstr(ptr %text25, ptr @str.41)
  %is_found27 = icmp ne ptr %strstr_result26, null
  %contains28 = zext i1 %is_found27 to i32
  store i32 %contains28, ptr %c2, align 4
  %text29 = load ptr, ptr %text, align 8
  %strstr_result30 = call ptr @strstr(ptr %text29, ptr @str.42)
  %is_found31 = icmp ne ptr %strstr_result30, null
  %contains32 = zext i1 %is_found31 to i32
  store i32 %contains32, ptr %c3, align 4
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %c133 = load i32, ptr %c1, align 4
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.46, i32 %c133)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.47)
  %c234 = load i32, ptr %c2, align 4
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.48, i32 %c234)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.49)
  %c335 = load i32, ptr %c3, align 4
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.50, i32 %c335)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.51)
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.53, ptr @str.52)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.54)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i64 @strlen(ptr)

declare i32 @strcmp(ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

declare ptr @strcat(ptr, ptr)

declare ptr @strcpy(ptr, ptr)

declare i32 @sprintf(ptr, ptr, ...)

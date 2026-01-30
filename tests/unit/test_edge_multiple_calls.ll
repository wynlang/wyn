; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.1 = private unnamed_addr constant [2 x i8] c"H\00", align 1
@str.2 = private unnamed_addr constant [2 x i8] c"e\00", align 1
@str.3 = private unnamed_addr constant [2 x i8] c"l\00", align 1
@str.4 = private unnamed_addr constant [2 x i8] c"o\00", align 1
@str.5 = private unnamed_addr constant [2 x i8] c"W\00", align 1
@str.6 = private unnamed_addr constant [2 x i8] c"r\00", align 1
@str.7 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@str.8 = private unnamed_addr constant [2 x i8] c"x\00", align 1
@str.9 = private unnamed_addr constant [19 x i8] c"Multiple contains:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.17 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.22 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.23 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.25 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.26 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.27 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.28 = private unnamed_addr constant [20 x i8] c"Mixed methods work:\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.36 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@typename = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.37 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@typename.38 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@str.39 = private unnamed_addr constant [7 x i8] c"Types:\00", align 1
@fmt.40 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.41 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.42 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.43 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.46 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.47 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %t3 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %length = alloca i64, align 8
  %ends = alloca i32, align 4
  %starts = alloca i32, align 4
  %lower41 = alloca ptr, align 8
  %upper39 = alloca ptr, align 8
  %c8 = alloca i32, align 4
  %c7 = alloca i32, align 4
  %c6 = alloca i32, align 4
  %c5 = alloca i32, align 4
  %c4 = alloca i32, align 4
  %c3 = alloca i32, align 4
  %c2 = alloca i32, align 4
  %c1 = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.1)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %c1, align 4
  %text2 = load ptr, ptr %text, align 8
  %strstr_result3 = call ptr @strstr(ptr %text2, ptr @str.2)
  %is_found4 = icmp ne ptr %strstr_result3, null
  %contains5 = zext i1 %is_found4 to i32
  store i32 %contains5, ptr %c2, align 4
  %text6 = load ptr, ptr %text, align 8
  %strstr_result7 = call ptr @strstr(ptr %text6, ptr @str.3)
  %is_found8 = icmp ne ptr %strstr_result7, null
  %contains9 = zext i1 %is_found8 to i32
  store i32 %contains9, ptr %c3, align 4
  %text10 = load ptr, ptr %text, align 8
  %strstr_result11 = call ptr @strstr(ptr %text10, ptr @str.4)
  %is_found12 = icmp ne ptr %strstr_result11, null
  %contains13 = zext i1 %is_found12 to i32
  store i32 %contains13, ptr %c4, align 4
  %text14 = load ptr, ptr %text, align 8
  %strstr_result15 = call ptr @strstr(ptr %text14, ptr @str.5)
  %is_found16 = icmp ne ptr %strstr_result15, null
  %contains17 = zext i1 %is_found16 to i32
  store i32 %contains17, ptr %c5, align 4
  %text18 = load ptr, ptr %text, align 8
  %strstr_result19 = call ptr @strstr(ptr %text18, ptr @str.6)
  %is_found20 = icmp ne ptr %strstr_result19, null
  %contains21 = zext i1 %is_found20 to i32
  store i32 %contains21, ptr %c6, align 4
  %text22 = load ptr, ptr %text, align 8
  %strstr_result23 = call ptr @strstr(ptr %text22, ptr @str.7)
  %is_found24 = icmp ne ptr %strstr_result23, null
  %contains25 = zext i1 %is_found24 to i32
  store i32 %contains25, ptr %c7, align 4
  %text26 = load ptr, ptr %text, align 8
  %strstr_result27 = call ptr @strstr(ptr %text26, ptr @str.8)
  %is_found28 = icmp ne ptr %strstr_result27, null
  %contains29 = zext i1 %is_found28 to i32
  store i32 %contains29, ptr %c8, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.9)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %c130 = load i32, ptr %c1, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.10, i32 %c130)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %c231 = load i32, ptr %c2, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.12, i32 %c231)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.13)
  %c332 = load i32, ptr %c3, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.14, i32 %c332)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.15)
  %c433 = load i32, ptr %c4, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.16, i32 %c433)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.17)
  %c534 = load i32, ptr %c5, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.18, i32 %c534)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %c635 = load i32, ptr %c6, align 4
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.20, i32 %c635)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %c736 = load i32, ptr %c7, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.22, i32 %c736)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.23)
  %c837 = load i32, ptr %c8, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.24, i32 %c837)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.25)
  %text38 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text38)
  store ptr %upper, ptr %upper39, align 8
  %text40 = load ptr, ptr %text, align 8
  %lower = call ptr @wyn_string_lower(ptr %text40)
  store ptr %lower, ptr %lower41, align 8
  %text42 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.26)
  %strncmp = call i32 @strncmp(ptr %text42, ptr @str.26, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts, align 4
  %text43 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text43)
  %suffix_len = call i64 @strlen(ptr @str.27)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text43, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.27)
  %is_equal44 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal44 to i32
  store i32 %ends_with, ptr %ends, align 4
  %text45 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text45)
  store i64 %strlen, ptr %length, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %starts46 = load i32, ptr %starts, align 4
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.31, i32 %starts46)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %ends47 = load i32, ptr %ends, align 4
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.33, i32 %ends47)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.34)
  %length48 = load i64, ptr %length, align 4
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.35, i64 %length48)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.36)
  %text49 = load ptr, ptr %text, align 8
  store ptr @typename, ptr %t1, align 8
  %c150 = load i32, ptr %c1, align 4
  store ptr @typename.37, ptr %t2, align 8
  %length51 = load i64, ptr %length, align 4
  store ptr @typename.38, ptr %t3, align 8
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.40, ptr @str.39)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.41)
  %t152 = load ptr, ptr %t1, align 8
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.42, ptr %t152)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.43)
  %t253 = load ptr, ptr %t2, align 8
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr %t253)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %t354 = load ptr, ptr %t3, align 8
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.46, ptr %t354)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.47)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i64 @strlen(ptr)

declare i32 @strcmp(ptr, ptr)

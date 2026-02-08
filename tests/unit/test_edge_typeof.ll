; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@typename = private unnamed_addr constant [4 x i8] c"int\00", align 1
@typename.1 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.2 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@typename.3 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@str.4 = private unnamed_addr constant [12 x i8] c"Type of 42:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [16 x i8] c"Type of string:\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [11 x i8] c"Type of 0:\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [13 x i8] c"Type of -10:\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.22 = private unnamed_addr constant [2 x i8] c"a\00", align 1
@str.23 = private unnamed_addr constant [2 x i8] c"b\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@typename.25 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.26 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.27 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@str.28 = private unnamed_addr constant [23 x i8] c"Type of concat result:\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.33 = private unnamed_addr constant [22 x i8] c"Type of upper result:\00", align 1
@fmt.34 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.35 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.38 = private unnamed_addr constant [26 x i8] c"Type of to_string result:\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@typename.43 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@str.44 = private unnamed_addr constant [20 x i8] c"Type of len result:\00", align 1
@fmt.45 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.46 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.47 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.48 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.49 = private unnamed_addr constant [2 x i8] c"h\00", align 1
@typename.50 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@str.51 = private unnamed_addr constant [25 x i8] c"Type of contains result:\00", align 1
@fmt.52 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.53 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.54 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.55 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %t9 = alloca ptr, align 8
  %contains_result = alloca i32, align 4
  %t8 = alloca ptr, align 8
  %length = alloca i64, align 8
  %t7 = alloca ptr, align 8
  %t6 = alloca ptr, align 8
  %t5 = alloca ptr, align 8
  %to_str = alloca ptr, align 8
  %upper10 = alloca ptr, align 8
  %concat = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %negative = alloca i32, align 4
  %zero = alloca i32, align 4
  %text = alloca ptr, align 8
  %num = alloca i32, align 4
  store i32 42, ptr %num, align 4
  store ptr @str, ptr %text, align 8
  store i32 0, ptr %zero, align 4
  store i32 -10, ptr %negative, align 4
  %num1 = load i32, ptr %num, align 4
  store ptr @typename, ptr %t1, align 8
  %text2 = load ptr, ptr %text, align 8
  store ptr @typename.1, ptr %t2, align 8
  %zero3 = load i32, ptr %zero, align 4
  store ptr @typename.2, ptr %t3, align 8
  %negative4 = load i32, ptr %negative, align 4
  store ptr @typename.3, ptr %t4, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.4)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %t15 = load ptr, ptr %t1, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr %t15)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %t26 = load ptr, ptr %t2, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr %t26)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.14)
  %t37 = load ptr, ptr %t3, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.15, ptr %t37)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %t48 = load ptr, ptr %t4, align 8
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr %t48)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %len1 = call i64 @strlen(ptr @str.22)
  %len2 = call i64 @strlen(ptr @str.23)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %16 = call ptr @strcpy(ptr %result, ptr @str.22)
  %17 = call ptr @strcat(ptr %result, ptr @str.23)
  store ptr %result, ptr %concat, align 8
  %text9 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text9)
  store ptr %upper, ptr %upper10, align 8
  %num11 = load i32, ptr %num, align 4
  %buffer = call ptr @malloc(i64 32)
  %18 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt.24, i32 %num11)
  store ptr %buffer, ptr %to_str, align 8
  %concat12 = load ptr, ptr %concat, align 8
  store ptr @typename.25, ptr %t5, align 8
  %upper13 = load ptr, ptr %upper10, align 8
  store ptr @typename.26, ptr %t6, align 8
  %to_str14 = load ptr, ptr %to_str, align 8
  store ptr @typename.27, ptr %t7, align 8
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %t515 = load ptr, ptr %t5, align 8
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr %t515)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.34, ptr @str.33)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.35)
  %t616 = load ptr, ptr %t6, align 8
  %25 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr %t616)
  %26 = call i32 (ptr, ...) @printf(ptr @nl.37)
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.39, ptr @str.38)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.40)
  %t717 = load ptr, ptr %t7, align 8
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr %t717)
  %30 = call i32 (ptr, ...) @printf(ptr @nl.42)
  %text18 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text18)
  store i64 %strlen, ptr %length, align 4
  %length19 = load i64, ptr %length, align 4
  store ptr @typename.43, ptr %t8, align 8
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.45, ptr @str.44)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.46)
  %t820 = load ptr, ptr %t8, align 8
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.47, ptr %t820)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.48)
  %text21 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text21, ptr @str.49)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains_result, align 4
  %contains_result22 = load i32, ptr %contains_result, align 4
  store ptr @typename.50, ptr %t9, align 8
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.52, ptr @str.51)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.53)
  %t923 = load ptr, ptr %t9, align 8
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.54, ptr %t923)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.55)
  ret i32 0
}

declare ptr @strcat(ptr, ptr)

declare i64 @strlen(ptr)

declare ptr @strcpy(ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare i32 @sprintf(ptr, ptr, ...)

declare ptr @strstr(ptr, ptr)

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.1 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.2 = private unnamed_addr constant [21 x i8] c"Empty string length:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.6 = private unnamed_addr constant [23 x i8] c"Contains empty string:\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.12 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.13 = private unnamed_addr constant [19 x i8] c"Starts with empty:\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.17 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.18 = private unnamed_addr constant [17 x i8] c"Ends with empty:\00", align 1
@fmt.19 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.20 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [14 x i8] c"Empty + text:\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.25 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.27 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.28 = private unnamed_addr constant [14 x i8] c"Text + empty:\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.33 = private unnamed_addr constant [13 x i8] c"Empty upper:\00", align 1
@fmt.34 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.35 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.38 = private unnamed_addr constant [13 x i8] c"Empty lower:\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %lower_empty = alloca ptr, align 8
  %upper_empty = alloca ptr, align 8
  %concat2 = alloca ptr, align 8
  %concat1 = alloca ptr, align 8
  %ends_empty = alloca i32, align 4
  %starts_empty = alloca i32, align 4
  %contains_empty = alloca i32, align 4
  %len_empty = alloca i64, align 8
  %text = alloca ptr, align 8
  %empty = alloca ptr, align 8
  store ptr @str, ptr %empty, align 8
  store ptr @str.1, ptr %text, align 8
  %empty1 = load ptr, ptr %empty, align 8
  %strlen = call i64 @strlen(ptr %empty1)
  store i64 %strlen, ptr %len_empty, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.2)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %len_empty2 = load i64, ptr %len_empty, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.3, i64 %len_empty2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %text3 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text3, ptr @str.5)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains_empty, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %contains_empty4 = load i32, ptr %contains_empty, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.9, i32 %contains_empty4)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %text5 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.11)
  %strncmp = call i32 @strncmp(ptr %text5, ptr @str.11, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts_empty, align 4
  %text6 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text6)
  %suffix_len = call i64 @strlen(ptr @str.12)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text6, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.12)
  %is_equal7 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal7 to i32
  store i32 %ends_with, ptr %ends_empty, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.15)
  %starts_empty8 = load i32, ptr %starts_empty, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.16, i32 %starts_empty8)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.17)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.19, ptr @str.18)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.20)
  %ends_empty9 = load i32, ptr %ends_empty, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.21, i32 %ends_empty9)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %empty10 = load ptr, ptr %empty, align 8
  %text11 = load ptr, ptr %text, align 8
  %len1 = call i64 @strlen(ptr %empty10)
  %len2 = call i64 @strlen(ptr %text11)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %16 = call ptr @strcpy(ptr %result, ptr %empty10)
  %17 = call ptr @strcat(ptr %result, ptr %text11)
  store ptr %result, ptr %concat1, align 8
  %text12 = load ptr, ptr %text, align 8
  %empty13 = load ptr, ptr %empty, align 8
  %len114 = call i64 @strlen(ptr %text12)
  %len215 = call i64 @strlen(ptr %empty13)
  %total16 = add i64 %len114, %len215
  %size17 = add i64 %total16, 1
  %result18 = call ptr @malloc(i64 %size17)
  %18 = call ptr @strcpy(ptr %result18, ptr %text12)
  %19 = call ptr @strcat(ptr %result18, ptr %empty13)
  store ptr %result18, ptr %concat2, align 8
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.25)
  %concat119 = load ptr, ptr %concat1, align 8
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr %concat119)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.27)
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %concat220 = load ptr, ptr %concat2, align 8
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr %concat220)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %empty21 = load ptr, ptr %empty, align 8
  %upper = call ptr @wyn_string_upper(ptr %empty21)
  store ptr %upper, ptr %upper_empty, align 8
  %empty22 = load ptr, ptr %empty, align 8
  %lower = call ptr @wyn_string_lower(ptr %empty22)
  store ptr %lower, ptr %lower_empty, align 8
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.34, ptr @str.33)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.35)
  %upper_empty23 = load ptr, ptr %upper_empty, align 8
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr %upper_empty23)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.37)
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.39, ptr @str.38)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.40)
  %lower_empty24 = load ptr, ptr %lower_empty, align 8
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr %lower_empty24)
  %35 = call i32 (ptr, ...) @printf(ptr @nl.42)
  ret i32 0
}

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i32 @strcmp(ptr, ptr)

declare ptr @strcat(ptr, ptr)

declare ptr @strcpy(ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

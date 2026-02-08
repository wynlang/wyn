; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.1 = private unnamed_addr constant [2 x i8] c" \00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.3 = private unnamed_addr constant [15 x i8] c"Triple concat:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.5 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.6 = private unnamed_addr constant [20 x i8] c"Same string concat:\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [3 x i8] c"\\n\00", align 1
@str.12 = private unnamed_addr constant [28 x i8] c"Concat with newline length:\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [2 x i8] c"A\00", align 1
@str.18 = private unnamed_addr constant [2 x i8] c"B\00", align 1
@str.19 = private unnamed_addr constant [2 x i8] c"C\00", align 1
@str.20 = private unnamed_addr constant [2 x i8] c"D\00", align 1
@str.21 = private unnamed_addr constant [21 x i8] c"Multiple concat ops:\00", align 1
@fmt.22 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.23 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.25 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.26 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.27 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.28 = private unnamed_addr constant [11 x i8] c"HelloWorld\00", align 1
@str.29 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.30 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.31 = private unnamed_addr constant [22 x i8] c"Concat then contains:\00", align 1
@fmt.32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.33 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.34 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.35 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.36 = private unnamed_addr constant [20 x i8] c"Concat then starts:\00", align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.38 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.41 = private unnamed_addr constant [18 x i8] c"Concat then ends:\00", align 1
@fmt.42 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.43 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %ends_d = alloca i32, align 4
  %starts_h = alloca i32, align 4
  %contains_hw = alloca i32, align 4
  %concat_result = alloca ptr, align 8
  %c = alloca ptr, align 8
  %b = alloca ptr, align 8
  %a = alloca ptr, align 8
  %len_with_newline = alloca i64, align 8
  %with_newline = alloca ptr, align 8
  %newline = alloca ptr, align 8
  %double = alloca ptr, align 8
  %concat2 = alloca ptr, align 8
  %concat1 = alloca ptr, align 8
  %s3 = alloca ptr, align 8
  %s2 = alloca ptr, align 8
  %s1 = alloca ptr, align 8
  store ptr @str, ptr %s1, align 8
  store ptr @str.1, ptr %s2, align 8
  store ptr @str.2, ptr %s3, align 8
  %s11 = load ptr, ptr %s1, align 8
  %s22 = load ptr, ptr %s2, align 8
  %len1 = call i64 @strlen(ptr %s11)
  %len2 = call i64 @strlen(ptr %s22)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %0 = call ptr @strcpy(ptr %result, ptr %s11)
  %1 = call ptr @strcat(ptr %result, ptr %s22)
  store ptr %result, ptr %concat1, align 8
  %concat13 = load ptr, ptr %concat1, align 8
  %s34 = load ptr, ptr %s3, align 8
  %len15 = call i64 @strlen(ptr %concat13)
  %len26 = call i64 @strlen(ptr %s34)
  %total7 = add i64 %len15, %len26
  %size8 = add i64 %total7, 1
  %result9 = call ptr @malloc(i64 %size8)
  %2 = call ptr @strcpy(ptr %result9, ptr %concat13)
  %3 = call ptr @strcat(ptr %result9, ptr %s34)
  store ptr %result9, ptr %concat2, align 8
  %4 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.3)
  %5 = call i32 (ptr, ...) @printf(ptr @nl)
  %concat210 = load ptr, ptr %concat2, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr %concat210)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.5)
  %s111 = load ptr, ptr %s1, align 8
  %s112 = load ptr, ptr %s1, align 8
  %len113 = call i64 @strlen(ptr %s111)
  %len214 = call i64 @strlen(ptr %s112)
  %total15 = add i64 %len113, %len214
  %size16 = add i64 %total15, 1
  %result17 = call ptr @malloc(i64 %size16)
  %8 = call ptr @strcpy(ptr %result17, ptr %s111)
  %9 = call ptr @strcat(ptr %result17, ptr %s112)
  store ptr %result17, ptr %double, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %double18 = load ptr, ptr %double, align 8
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr %double18)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.10)
  store ptr @str.11, ptr %newline, align 8
  %s119 = load ptr, ptr %s1, align 8
  %newline20 = load ptr, ptr %newline, align 8
  %len121 = call i64 @strlen(ptr %s119)
  %len222 = call i64 @strlen(ptr %newline20)
  %total23 = add i64 %len121, %len222
  %size24 = add i64 %total23, 1
  %result25 = call ptr @malloc(i64 %size24)
  %14 = call ptr @strcpy(ptr %result25, ptr %s119)
  %15 = call ptr @strcat(ptr %result25, ptr %newline20)
  store ptr %result25, ptr %with_newline, align 8
  %with_newline26 = load ptr, ptr %with_newline, align 8
  %strlen = call i64 @strlen(ptr %with_newline26)
  store i64 %strlen, ptr %len_with_newline, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.14)
  %len_with_newline27 = load i64, ptr %len_with_newline, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.15, i64 %len_with_newline27)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %len128 = call i64 @strlen(ptr @str.17)
  %len229 = call i64 @strlen(ptr @str.18)
  %total30 = add i64 %len128, %len229
  %size31 = add i64 %total30, 1
  %result32 = call ptr @malloc(i64 %size31)
  %20 = call ptr @strcpy(ptr %result32, ptr @str.17)
  %21 = call ptr @strcat(ptr %result32, ptr @str.18)
  store ptr %result32, ptr %a, align 8
  %len133 = call i64 @strlen(ptr @str.19)
  %len234 = call i64 @strlen(ptr @str.20)
  %total35 = add i64 %len133, %len234
  %size36 = add i64 %total35, 1
  %result37 = call ptr @malloc(i64 %size36)
  %22 = call ptr @strcpy(ptr %result37, ptr @str.19)
  %23 = call ptr @strcat(ptr %result37, ptr @str.20)
  store ptr %result37, ptr %b, align 8
  %a38 = load ptr, ptr %a, align 8
  %b39 = load ptr, ptr %b, align 8
  %len140 = call i64 @strlen(ptr %a38)
  %len241 = call i64 @strlen(ptr %b39)
  %total42 = add i64 %len140, %len241
  %size43 = add i64 %total42, 1
  %result44 = call ptr @malloc(i64 %size43)
  %24 = call ptr @strcpy(ptr %result44, ptr %a38)
  %25 = call ptr @strcat(ptr %result44, ptr %b39)
  store ptr %result44, ptr %c, align 8
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.22, ptr @str.21)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.23)
  %c45 = load ptr, ptr %c, align 8
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr %c45)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.25)
  %len146 = call i64 @strlen(ptr @str.26)
  %len247 = call i64 @strlen(ptr @str.27)
  %total48 = add i64 %len146, %len247
  %size49 = add i64 %total48, 1
  %result50 = call ptr @malloc(i64 %size49)
  %30 = call ptr @strcpy(ptr %result50, ptr @str.26)
  %31 = call ptr @strcat(ptr %result50, ptr @str.27)
  store ptr %result50, ptr %concat_result, align 8
  %concat_result51 = load ptr, ptr %concat_result, align 8
  %strstr_result = call ptr @strstr(ptr %concat_result51, ptr @str.28)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains_hw, align 4
  %concat_result52 = load ptr, ptr %concat_result, align 8
  %prefix_len = call i64 @strlen(ptr @str.29)
  %strncmp = call i32 @strncmp(ptr %concat_result52, ptr @str.29, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts_h, align 4
  %concat_result53 = load ptr, ptr %concat_result, align 8
  %str_len = call i64 @strlen(ptr %concat_result53)
  %suffix_len = call i64 @strlen(ptr @str.30)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %concat_result53, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.30)
  %is_equal54 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal54 to i32
  store i32 %ends_with, ptr %ends_d, align 4
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.32, ptr @str.31)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.33)
  %contains_hw55 = load i32, ptr %contains_hw, align 4
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.34, i32 %contains_hw55)
  %35 = call i32 (ptr, ...) @printf(ptr @nl.35)
  %36 = call i32 (ptr, ...) @printf(ptr @fmt.37, ptr @str.36)
  %37 = call i32 (ptr, ...) @printf(ptr @nl.38)
  %starts_h56 = load i32, ptr %starts_h, align 4
  %38 = call i32 (ptr, ...) @printf(ptr @fmt.39, i32 %starts_h56)
  %39 = call i32 (ptr, ...) @printf(ptr @nl.40)
  %40 = call i32 (ptr, ...) @printf(ptr @fmt.42, ptr @str.41)
  %41 = call i32 (ptr, ...) @printf(ptr @nl.43)
  %ends_d57 = load i32, ptr %ends_d, align 4
  %42 = call i32 (ptr, ...) @printf(ptr @fmt.44, i32 %ends_d57)
  %43 = call i32 (ptr, ...) @printf(ptr @nl.45)
  ret i32 0
}

declare ptr @strcat(ptr, ptr)

declare i64 @strlen(ptr)

declare ptr @strcpy(ptr, ptr)

declare ptr @strstr(ptr, ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i32 @strcmp(ptr, ptr)

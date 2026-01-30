; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [22 x i8] c"Contains full string:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [2 x i8] c"H\00", align 1
@str.6 = private unnamed_addr constant [22 x i8] c"Contains single char:\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.11 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@str.12 = private unnamed_addr constant [20 x i8] c"Contains last char:\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [4 x i8] c"xyz\00", align 1
@str.18 = private unnamed_addr constant [16 x i8] c"Contains 'xyz':\00", align 1
@fmt.19 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.20 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [2 x i8] c"h\00", align 1
@str.24 = private unnamed_addr constant [24 x i8] c"Contains lowercase 'h':\00", align 1
@fmt.25 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.26 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.29 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.30 = private unnamed_addr constant [18 x i8] c"Starts with full:\00", align 1
@fmt.31 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.32 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.35 = private unnamed_addr constant [2 x i8] c"H\00", align 1
@str.36 = private unnamed_addr constant [17 x i8] c"Starts with 'H':\00", align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.38 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.39 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.40 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.41 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.42 = private unnamed_addr constant [16 x i8] c"Ends with full:\00", align 1
@fmt.43 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.44 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.45 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.46 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.47 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@str.48 = private unnamed_addr constant [15 x i8] c"Ends with 'd':\00", align 1
@fmt.49 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.50 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.51 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.52 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.53 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.54 = private unnamed_addr constant [21 x i8] c"Starts with 'World':\00", align 1
@fmt.55 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.56 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.57 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.58 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.59 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.60 = private unnamed_addr constant [19 x i8] c"Ends with 'Hello':\00", align 1
@fmt.61 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.62 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.63 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.64 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %not_ends = alloca i32, align 4
  %not_starts = alloca i32, align 4
  %ends_single = alloca i32, align 4
  %ends_full = alloca i32, align 4
  %starts_single = alloca i32, align 4
  %starts_full = alloca i32, align 4
  %lower_h = alloca i32, align 4
  %not_found = alloca i32, align 4
  %last = alloca i32, align 4
  %single = alloca i32, align 4
  %full_match = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.1)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %full_match, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.2)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %full_match2 = load i32, ptr %full_match, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %full_match2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %text3 = load ptr, ptr %text, align 8
  %strstr_result4 = call ptr @strstr(ptr %text3, ptr @str.5)
  %is_found5 = icmp ne ptr %strstr_result4, null
  %contains6 = zext i1 %is_found5 to i32
  store i32 %contains6, ptr %single, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %single7 = load i32, ptr %single, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.9, i32 %single7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %text8 = load ptr, ptr %text, align 8
  %strstr_result9 = call ptr @strstr(ptr %text8, ptr @str.11)
  %is_found10 = icmp ne ptr %strstr_result9, null
  %contains11 = zext i1 %is_found10 to i32
  store i32 %contains11, ptr %last, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.14)
  %last12 = load i32, ptr %last, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.15, i32 %last12)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %text13 = load ptr, ptr %text, align 8
  %strstr_result14 = call ptr @strstr(ptr %text13, ptr @str.17)
  %is_found15 = icmp ne ptr %strstr_result14, null
  %contains16 = zext i1 %is_found15 to i32
  store i32 %contains16, ptr %not_found, align 4
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.19, ptr @str.18)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.20)
  %not_found17 = load i32, ptr %not_found, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.21, i32 %not_found17)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %text18 = load ptr, ptr %text, align 8
  %strstr_result19 = call ptr @strstr(ptr %text18, ptr @str.23)
  %is_found20 = icmp ne ptr %strstr_result19, null
  %contains21 = zext i1 %is_found20 to i32
  store i32 %contains21, ptr %lower_h, align 4
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.25, ptr @str.24)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.26)
  %lower_h22 = load i32, ptr %lower_h, align 4
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.27, i32 %lower_h22)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.28)
  %text23 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.29)
  %strncmp = call i32 @strncmp(ptr %text23, ptr @str.29, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts_full, align 4
  %20 = call i32 (ptr, ...) @printf(ptr @fmt.31, ptr @str.30)
  %21 = call i32 (ptr, ...) @printf(ptr @nl.32)
  %starts_full24 = load i32, ptr %starts_full, align 4
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.33, i32 %starts_full24)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.34)
  %text25 = load ptr, ptr %text, align 8
  %prefix_len26 = call i64 @strlen(ptr @str.35)
  %strncmp27 = call i32 @strncmp(ptr %text25, ptr @str.35, i64 %prefix_len26)
  %is_equal28 = icmp eq i32 %strncmp27, 0
  %starts_with29 = zext i1 %is_equal28 to i32
  store i32 %starts_with29, ptr %starts_single, align 4
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.37, ptr @str.36)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.38)
  %starts_single30 = load i32, ptr %starts_single, align 4
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.39, i32 %starts_single30)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.40)
  %text31 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text31)
  %suffix_len = call i64 @strlen(ptr @str.41)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text31, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.41)
  %is_equal32 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal32 to i32
  store i32 %ends_with, ptr %ends_full, align 4
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.43, ptr @str.42)
  %29 = call i32 (ptr, ...) @printf(ptr @nl.44)
  %ends_full33 = load i32, ptr %ends_full, align 4
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.45, i32 %ends_full33)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.46)
  %text34 = load ptr, ptr %text, align 8
  %str_len35 = call i64 @strlen(ptr %text34)
  %suffix_len36 = call i64 @strlen(ptr @str.47)
  %offset37 = sub i64 %str_len35, %suffix_len36
  %str_end38 = getelementptr i8, ptr %text34, i64 %offset37
  %strcmp39 = call i32 @strcmp(ptr %str_end38, ptr @str.47)
  %is_equal40 = icmp eq i32 %strcmp39, 0
  %ends_with41 = zext i1 %is_equal40 to i32
  store i32 %ends_with41, ptr %ends_single, align 4
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.49, ptr @str.48)
  %33 = call i32 (ptr, ...) @printf(ptr @nl.50)
  %ends_single42 = load i32, ptr %ends_single, align 4
  %34 = call i32 (ptr, ...) @printf(ptr @fmt.51, i32 %ends_single42)
  %35 = call i32 (ptr, ...) @printf(ptr @nl.52)
  %text43 = load ptr, ptr %text, align 8
  %prefix_len44 = call i64 @strlen(ptr @str.53)
  %strncmp45 = call i32 @strncmp(ptr %text43, ptr @str.53, i64 %prefix_len44)
  %is_equal46 = icmp eq i32 %strncmp45, 0
  %starts_with47 = zext i1 %is_equal46 to i32
  store i32 %starts_with47, ptr %not_starts, align 4
  %36 = call i32 (ptr, ...) @printf(ptr @fmt.55, ptr @str.54)
  %37 = call i32 (ptr, ...) @printf(ptr @nl.56)
  %not_starts48 = load i32, ptr %not_starts, align 4
  %38 = call i32 (ptr, ...) @printf(ptr @fmt.57, i32 %not_starts48)
  %39 = call i32 (ptr, ...) @printf(ptr @nl.58)
  %text49 = load ptr, ptr %text, align 8
  %str_len50 = call i64 @strlen(ptr %text49)
  %suffix_len51 = call i64 @strlen(ptr @str.59)
  %offset52 = sub i64 %str_len50, %suffix_len51
  %str_end53 = getelementptr i8, ptr %text49, i64 %offset52
  %strcmp54 = call i32 @strcmp(ptr %str_end53, ptr @str.59)
  %is_equal55 = icmp eq i32 %strcmp54, 0
  %ends_with56 = zext i1 %is_equal55 to i32
  store i32 %ends_with56, ptr %not_ends, align 4
  %40 = call i32 (ptr, ...) @printf(ptr @fmt.61, ptr @str.60)
  %41 = call i32 (ptr, ...) @printf(ptr @nl.62)
  %not_ends57 = load i32, ptr %not_ends, align 4
  %42 = call i32 (ptr, ...) @printf(ptr @fmt.63, i32 %not_ends57)
  %43 = call i32 (ptr, ...) @printf(ptr @nl.64)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i64 @strlen(ptr)

declare i32 @strcmp(ptr, ptr)

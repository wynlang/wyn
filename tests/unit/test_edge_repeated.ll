; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.1 = private unnamed_addr constant [2 x i8] c"H\00", align 1
@str.2 = private unnamed_addr constant [2 x i8] c"e\00", align 1
@str.3 = private unnamed_addr constant [2 x i8] c"l\00", align 1
@str.4 = private unnamed_addr constant [2 x i8] c"l\00", align 1
@str.5 = private unnamed_addr constant [2 x i8] c"o\00", align 1
@str.6 = private unnamed_addr constant [2 x i8] c" \00", align 1
@str.7 = private unnamed_addr constant [2 x i8] c"W\00", align 1
@str.8 = private unnamed_addr constant [2 x i8] c"o\00", align 1
@str.9 = private unnamed_addr constant [2 x i8] c"r\00", align 1
@str.10 = private unnamed_addr constant [2 x i8] c"l\00", align 1
@str.11 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@str.12 = private unnamed_addr constant [2 x i8] c"x\00", align 1
@str.13 = private unnamed_addr constant [2 x i8] c"y\00", align 1
@str.14 = private unnamed_addr constant [2 x i8] c"z\00", align 1
@str.15 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.16 = private unnamed_addr constant [28 x i8] c"15 contains calls completed\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [30 x i8] c"6 upper/lower calls completed\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.20 = private unnamed_addr constant [2 x i8] c"H\00", align 1
@str.21 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@str.22 = private unnamed_addr constant [3 x i8] c"He\00", align 1
@str.23 = private unnamed_addr constant [3 x i8] c"ld\00", align 1
@str.24 = private unnamed_addr constant [4 x i8] c"Hel\00", align 1
@str.25 = private unnamed_addr constant [4 x i8] c"rld\00", align 1
@str.26 = private unnamed_addr constant [30 x i8] c"6 starts/ends calls completed\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.29 = private unnamed_addr constant [22 x i8] c"5 len calls completed\00", align 1
@fmt.30 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.31 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@typename = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.32 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.33 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.34 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.35 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@str.36 = private unnamed_addr constant [25 x i8] c"5 typeof calls completed\00", align 1
@fmt.37 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.38 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.39 = private unnamed_addr constant [36 x i8] c"All repeated operations successful!\00", align 1
@fmt.40 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.41 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %t5 = alloca ptr, align 8
  %t4 = alloca ptr, align 8
  %t3 = alloca ptr, align 8
  %t2 = alloca ptr, align 8
  %t1 = alloca ptr, align 8
  %len5 = alloca i64, align 8
  %len4 = alloca i64, align 8
  %len3 = alloca i64, align 8
  %len2 = alloca i64, align 8
  %len1 = alloca i64, align 8
  %e3 = alloca i32, align 4
  %s3 = alloca i32, align 4
  %e2 = alloca i32, align 4
  %s2 = alloca i32, align 4
  %e1 = alloca i32, align 4
  %s1 = alloca i32, align 4
  %l3 = alloca ptr, align 8
  %u3 = alloca ptr, align 8
  %l2 = alloca ptr, align 8
  %u2 = alloca ptr, align 8
  %l1 = alloca ptr, align 8
  %u1 = alloca ptr, align 8
  %r15 = alloca i32, align 4
  %r14 = alloca i32, align 4
  %r13 = alloca i32, align 4
  %r12 = alloca i32, align 4
  %r11 = alloca i32, align 4
  %r10 = alloca i32, align 4
  %r9 = alloca i32, align 4
  %r8 = alloca i32, align 4
  %r7 = alloca i32, align 4
  %r6 = alloca i32, align 4
  %r5 = alloca i32, align 4
  %r4 = alloca i32, align 4
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.1)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %r1, align 4
  %text2 = load ptr, ptr %text, align 8
  %strstr_result3 = call ptr @strstr(ptr %text2, ptr @str.2)
  %is_found4 = icmp ne ptr %strstr_result3, null
  %contains5 = zext i1 %is_found4 to i32
  store i32 %contains5, ptr %r2, align 4
  %text6 = load ptr, ptr %text, align 8
  %strstr_result7 = call ptr @strstr(ptr %text6, ptr @str.3)
  %is_found8 = icmp ne ptr %strstr_result7, null
  %contains9 = zext i1 %is_found8 to i32
  store i32 %contains9, ptr %r3, align 4
  %text10 = load ptr, ptr %text, align 8
  %strstr_result11 = call ptr @strstr(ptr %text10, ptr @str.4)
  %is_found12 = icmp ne ptr %strstr_result11, null
  %contains13 = zext i1 %is_found12 to i32
  store i32 %contains13, ptr %r4, align 4
  %text14 = load ptr, ptr %text, align 8
  %strstr_result15 = call ptr @strstr(ptr %text14, ptr @str.5)
  %is_found16 = icmp ne ptr %strstr_result15, null
  %contains17 = zext i1 %is_found16 to i32
  store i32 %contains17, ptr %r5, align 4
  %text18 = load ptr, ptr %text, align 8
  %strstr_result19 = call ptr @strstr(ptr %text18, ptr @str.6)
  %is_found20 = icmp ne ptr %strstr_result19, null
  %contains21 = zext i1 %is_found20 to i32
  store i32 %contains21, ptr %r6, align 4
  %text22 = load ptr, ptr %text, align 8
  %strstr_result23 = call ptr @strstr(ptr %text22, ptr @str.7)
  %is_found24 = icmp ne ptr %strstr_result23, null
  %contains25 = zext i1 %is_found24 to i32
  store i32 %contains25, ptr %r7, align 4
  %text26 = load ptr, ptr %text, align 8
  %strstr_result27 = call ptr @strstr(ptr %text26, ptr @str.8)
  %is_found28 = icmp ne ptr %strstr_result27, null
  %contains29 = zext i1 %is_found28 to i32
  store i32 %contains29, ptr %r8, align 4
  %text30 = load ptr, ptr %text, align 8
  %strstr_result31 = call ptr @strstr(ptr %text30, ptr @str.9)
  %is_found32 = icmp ne ptr %strstr_result31, null
  %contains33 = zext i1 %is_found32 to i32
  store i32 %contains33, ptr %r9, align 4
  %text34 = load ptr, ptr %text, align 8
  %strstr_result35 = call ptr @strstr(ptr %text34, ptr @str.10)
  %is_found36 = icmp ne ptr %strstr_result35, null
  %contains37 = zext i1 %is_found36 to i32
  store i32 %contains37, ptr %r10, align 4
  %text38 = load ptr, ptr %text, align 8
  %strstr_result39 = call ptr @strstr(ptr %text38, ptr @str.11)
  %is_found40 = icmp ne ptr %strstr_result39, null
  %contains41 = zext i1 %is_found40 to i32
  store i32 %contains41, ptr %r11, align 4
  %text42 = load ptr, ptr %text, align 8
  %strstr_result43 = call ptr @strstr(ptr %text42, ptr @str.12)
  %is_found44 = icmp ne ptr %strstr_result43, null
  %contains45 = zext i1 %is_found44 to i32
  store i32 %contains45, ptr %r12, align 4
  %text46 = load ptr, ptr %text, align 8
  %strstr_result47 = call ptr @strstr(ptr %text46, ptr @str.13)
  %is_found48 = icmp ne ptr %strstr_result47, null
  %contains49 = zext i1 %is_found48 to i32
  store i32 %contains49, ptr %r13, align 4
  %text50 = load ptr, ptr %text, align 8
  %strstr_result51 = call ptr @strstr(ptr %text50, ptr @str.14)
  %is_found52 = icmp ne ptr %strstr_result51, null
  %contains53 = zext i1 %is_found52 to i32
  store i32 %contains53, ptr %r14, align 4
  %text54 = load ptr, ptr %text, align 8
  %strstr_result55 = call ptr @strstr(ptr %text54, ptr @str.15)
  %is_found56 = icmp ne ptr %strstr_result55, null
  %contains57 = zext i1 %is_found56 to i32
  store i32 %contains57, ptr %r15, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.16)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %text58 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text58)
  store ptr %upper, ptr %u1, align 8
  %text59 = load ptr, ptr %text, align 8
  %lower = call ptr @wyn_string_lower(ptr %text59)
  store ptr %lower, ptr %l1, align 8
  %text60 = load ptr, ptr %text, align 8
  %upper61 = call ptr @wyn_string_upper(ptr %text60)
  store ptr %upper61, ptr %u2, align 8
  %text62 = load ptr, ptr %text, align 8
  %lower63 = call ptr @wyn_string_lower(ptr %text62)
  store ptr %lower63, ptr %l2, align 8
  %text64 = load ptr, ptr %text, align 8
  %upper65 = call ptr @wyn_string_upper(ptr %text64)
  store ptr %upper65, ptr %u3, align 8
  %text66 = load ptr, ptr %text, align 8
  %lower67 = call ptr @wyn_string_lower(ptr %text66)
  store ptr %lower67, ptr %l3, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %text68 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.20)
  %strncmp = call i32 @strncmp(ptr %text68, ptr @str.20, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %s1, align 4
  %text69 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text69)
  %suffix_len = call i64 @strlen(ptr @str.21)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text69, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.21)
  %is_equal70 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal70 to i32
  store i32 %ends_with, ptr %e1, align 4
  %text71 = load ptr, ptr %text, align 8
  %prefix_len72 = call i64 @strlen(ptr @str.22)
  %strncmp73 = call i32 @strncmp(ptr %text71, ptr @str.22, i64 %prefix_len72)
  %is_equal74 = icmp eq i32 %strncmp73, 0
  %starts_with75 = zext i1 %is_equal74 to i32
  store i32 %starts_with75, ptr %s2, align 4
  %text76 = load ptr, ptr %text, align 8
  %str_len77 = call i64 @strlen(ptr %text76)
  %suffix_len78 = call i64 @strlen(ptr @str.23)
  %offset79 = sub i64 %str_len77, %suffix_len78
  %str_end80 = getelementptr i8, ptr %text76, i64 %offset79
  %strcmp81 = call i32 @strcmp(ptr %str_end80, ptr @str.23)
  %is_equal82 = icmp eq i32 %strcmp81, 0
  %ends_with83 = zext i1 %is_equal82 to i32
  store i32 %ends_with83, ptr %e2, align 4
  %text84 = load ptr, ptr %text, align 8
  %prefix_len85 = call i64 @strlen(ptr @str.24)
  %strncmp86 = call i32 @strncmp(ptr %text84, ptr @str.24, i64 %prefix_len85)
  %is_equal87 = icmp eq i32 %strncmp86, 0
  %starts_with88 = zext i1 %is_equal87 to i32
  store i32 %starts_with88, ptr %s3, align 4
  %text89 = load ptr, ptr %text, align 8
  %str_len90 = call i64 @strlen(ptr %text89)
  %suffix_len91 = call i64 @strlen(ptr @str.25)
  %offset92 = sub i64 %str_len90, %suffix_len91
  %str_end93 = getelementptr i8, ptr %text89, i64 %offset92
  %strcmp94 = call i32 @strcmp(ptr %str_end93, ptr @str.25)
  %is_equal95 = icmp eq i32 %strcmp94, 0
  %ends_with96 = zext i1 %is_equal95 to i32
  store i32 %ends_with96, ptr %e3, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.27, ptr @str.26)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.28)
  %text97 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text97)
  store i64 %strlen, ptr %len1, align 4
  %text98 = load ptr, ptr %text, align 8
  %strlen99 = call i64 @strlen(ptr %text98)
  store i64 %strlen99, ptr %len2, align 4
  %text100 = load ptr, ptr %text, align 8
  %strlen101 = call i64 @strlen(ptr %text100)
  store i64 %strlen101, ptr %len3, align 4
  %text102 = load ptr, ptr %text, align 8
  %strlen103 = call i64 @strlen(ptr %text102)
  store i64 %strlen103, ptr %len4, align 4
  %text104 = load ptr, ptr %text, align 8
  %strlen105 = call i64 @strlen(ptr %text104)
  store i64 %strlen105, ptr %len5, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.30, ptr @str.29)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.31)
  %text106 = load ptr, ptr %text, align 8
  store ptr @typename, ptr %t1, align 8
  %text107 = load ptr, ptr %text, align 8
  store ptr @typename.32, ptr %t2, align 8
  %text108 = load ptr, ptr %text, align 8
  store ptr @typename.33, ptr %t3, align 8
  %text109 = load ptr, ptr %text, align 8
  store ptr @typename.34, ptr %t4, align 8
  %text110 = load ptr, ptr %text, align 8
  store ptr @typename.35, ptr %t5, align 8
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.37, ptr @str.36)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.38)
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.40, ptr @str.39)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.41)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i64 @strlen(ptr)

declare i32 @strcmp(ptr, ptr)

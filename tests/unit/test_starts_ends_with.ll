; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.4 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.5 = private unnamed_addr constant [22 x i8] c"starts_with('Hello'):\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.8 = private unnamed_addr constant [22 x i8] c"starts_with('World'):\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [20 x i8] c"ends_with('World'):\00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.15 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.17 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.18 = private unnamed_addr constant [20 x i8] c"ends_with('Hello'):\00", align 1
@fmt.19 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.20 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [32 x i8] c"\E2\9C\93 starts_with/ends_with work!\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.25 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.26 = private unnamed_addr constant [33 x i8] c"\E2\9C\97 starts_with/ends_with failed\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %ends_hello = alloca i32, align 4
  %ends_world = alloca i32, align 4
  %starts_world = alloca i32, align 4
  %starts_hello = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.1)
  %strncmp = call i32 @strncmp(ptr %text1, ptr @str.1, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts_hello, align 4
  %text2 = load ptr, ptr %text, align 8
  %prefix_len3 = call i64 @strlen(ptr @str.2)
  %strncmp4 = call i32 @strncmp(ptr %text2, ptr @str.2, i64 %prefix_len3)
  %is_equal5 = icmp eq i32 %strncmp4, 0
  %starts_with6 = zext i1 %is_equal5 to i32
  store i32 %starts_with6, ptr %starts_world, align 4
  %text7 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text7)
  %suffix_len = call i64 @strlen(ptr @str.3)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text7, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.3)
  %is_equal8 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal8 to i32
  store i32 %ends_with, ptr %ends_world, align 4
  %text9 = load ptr, ptr %text, align 8
  %str_len10 = call i64 @strlen(ptr %text9)
  %suffix_len11 = call i64 @strlen(ptr @str.4)
  %offset12 = sub i64 %str_len10, %suffix_len11
  %str_end13 = getelementptr i8, ptr %text9, i64 %offset12
  %strcmp14 = call i32 @strcmp(ptr %str_end13, ptr @str.4)
  %is_equal15 = icmp eq i32 %strcmp14, 0
  %ends_with16 = zext i1 %is_equal15 to i32
  store i32 %ends_with16, ptr %ends_hello, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.5)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %starts_hello17 = load i32, ptr %starts_hello, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.6, i32 %starts_hello17)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr @str.8)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.10)
  %starts_world18 = load i32, ptr %starts_world, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.11, i32 %starts_world18)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.12)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.15)
  %ends_world19 = load i32, ptr %ends_world, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.16, i32 %ends_world19)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.17)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.19, ptr @str.18)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.20)
  %ends_hello20 = load i32, ptr %ends_hello, align 4
  %14 = call i32 (ptr, ...) @printf(ptr @fmt.21, i32 %ends_hello20)
  %15 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %starts_hello21 = load i32, ptr %starts_hello, align 4
  %icmp = icmp eq i32 %starts_hello21, 1
  %starts_world22 = load i32, ptr %starts_world, align 4
  %icmp23 = icmp eq i32 %starts_world22, 0
  %and = and i1 %icmp, %icmp23
  %ends_world24 = load i32, ptr %ends_world, align 4
  %icmp25 = icmp eq i32 %ends_world24, 1
  %and26 = and i1 %and, %icmp25
  %ends_hello27 = load i32, ptr %ends_hello, align 4
  %icmp28 = icmp eq i32 %ends_hello27, 0
  %and29 = and i1 %and26, %icmp28
  br i1 %and29, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %16 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %17 = call i32 (ptr, ...) @printf(ptr @nl.25)
  ret i32 0

if.else:                                          ; preds = %entry
  %18 = call i32 (ptr, ...) @printf(ptr @fmt.27, ptr @str.26)
  %19 = call i32 (ptr, ...) @printf(ptr @nl.28)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i32 @strncmp(ptr, ptr, i64)

declare i64 @strlen(ptr)

declare i32 @strcmp(ptr, ptr)

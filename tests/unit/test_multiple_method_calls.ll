; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [28 x i8] c"Test: Multiple method calls\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.3 = private unnamed_addr constant [19 x i8] c"Contains 'Hello': \00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.8 = private unnamed_addr constant [19 x i8] c"Contains 'World': \00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [4 x i8] c"xyz\00", align 1
@str.13 = private unnamed_addr constant [17 x i8] c"Contains 'xyz': \00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.16 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.17 = private unnamed_addr constant [32 x i8] c"\E2\9C\93 Multiple method calls work!\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %has_xyz = alloca i32, align 4
  %has_world = alloca i32, align 4
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
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr @str.3)
  %has_hello2 = load i32, ptr %has_hello, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 %has_hello2)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %text3 = load ptr, ptr %text, align 8
  %strstr_result4 = call ptr @strstr(ptr %text3, ptr @str.7)
  %is_found5 = icmp ne ptr %strstr_result4, null
  %contains6 = zext i1 %is_found5 to i32
  store i32 %contains6, ptr %has_world, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr @str.8)
  %has_world7 = load i32, ptr %has_world, align 4
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.10, i32 %has_world7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %text8 = load ptr, ptr %text, align 8
  %strstr_result9 = call ptr @strstr(ptr %text8, ptr @str.12)
  %is_found10 = icmp ne ptr %strstr_result9, null
  %contains11 = zext i1 %is_found10 to i32
  store i32 %contains11, ptr %has_xyz, align 4
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %has_xyz12 = load i32, ptr %has_xyz, align 4
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.15, i32 %has_xyz12)
  %10 = call i32 (ptr, ...) @printf(ptr @nl.16)
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.18, ptr @str.17)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.19)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

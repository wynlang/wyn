; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [28 x i8] c"Testing string functions...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.3 = private unnamed_addr constant [19 x i8] c"Contains 'World': \00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [21 x i8] c"String tests passed!\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %contains3 = alloca i32, align 4
  %needle = alloca ptr, align 8
  %haystack = alloca ptr, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  store ptr @str.1, ptr %haystack, align 8
  store ptr @str.2, ptr %needle, align 8
  %haystack1 = load ptr, ptr %haystack, align 8
  %needle2 = load ptr, ptr %needle, align 8
  %strstr_result = call ptr @strstr(ptr %haystack1, ptr %needle2)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains3, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr @str.3)
  %contains4 = load i32, ptr %contains3, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 %contains4)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.9)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [23 x i8] c"Test: Two method calls\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.8 = private unnamed_addr constant [6 x i8] c"Done!\00", align 1
@fmt.9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.10 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
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
  %has_hello2 = load i32, ptr %has_hello, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %has_hello2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %text3 = load ptr, ptr %text, align 8
  %strstr_result4 = call ptr @strstr(ptr %text3, ptr @str.5)
  %is_found5 = icmp ne ptr %strstr_result4, null
  %contains6 = zext i1 %is_found5 to i32
  store i32 %contains6, ptr %has_world, align 4
  %has_world7 = load i32, ptr %has_world, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.6, i32 %has_world7)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.9, ptr @str.8)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.10)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

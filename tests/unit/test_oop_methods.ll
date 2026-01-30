; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [29 x i8] c"Testing OOP method syntax...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@str.3 = private unnamed_addr constant [11 x i8] c"Contains: \00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [31 x i8] c"\E2\9C\93 OOP method syntax working!\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %has_world = alloca i32, align 4
  %text = alloca ptr, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  store ptr @str.1, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.2)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_world, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr @str.3)
  %has_world2 = load i32, ptr %has_world, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 %has_world2)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.9)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

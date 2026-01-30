; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@str.1 = private unnamed_addr constant [10 x i8] c"Original:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [7 x i8] c"Upper:\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.9 = private unnamed_addr constant [7 x i8] c"Lower:\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.12 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %lower4 = alloca ptr, align 8
  %upper2 = alloca ptr, align 8
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store ptr %upper, ptr %upper2, align 8
  %text3 = load ptr, ptr %text, align 8
  %lower = call ptr @wyn_string_lower(ptr %text3)
  store ptr %lower, ptr %lower4, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.1)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %text5 = load ptr, ptr %text, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr %text5)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %upper6 = load ptr, ptr %upper2, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr %upper6)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr @str.9)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.11)
  %lower7 = load ptr, ptr %lower4, align 8
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.12, ptr %lower7)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.13)
  ret i32 0
}

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

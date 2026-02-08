; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.1 = private unnamed_addr constant [7 x i8] c" World\00", align 1
@str.2 = private unnamed_addr constant [4 x i8] c"s1:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [4 x i8] c"s2:\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [14 x i8] c"Concatenated:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result3 = alloca ptr, align 8
  %s2 = alloca ptr, align 8
  %s1 = alloca ptr, align 8
  store ptr @str, ptr %s1, align 8
  store ptr @str.1, ptr %s2, align 8
  %s11 = load ptr, ptr %s1, align 8
  %s22 = load ptr, ptr %s2, align 8
  %len1 = call i64 @strlen(ptr %s11)
  %len2 = call i64 @strlen(ptr %s22)
  %total = add i64 %len1, %len2
  %size = add i64 %total, 1
  %result = call ptr @malloc(i64 %size)
  %0 = call ptr @strcpy(ptr %result, ptr %s11)
  %1 = call ptr @strcat(ptr %result, ptr %s22)
  store ptr %result, ptr %result3, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl)
  %s14 = load ptr, ptr %s1, align 8
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.3, ptr %s14)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %s25 = load ptr, ptr %s2, align 8
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr %s25)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %11 = call i32 (ptr, ...) @printf(ptr @nl.12)
  %result6 = load ptr, ptr %result3, align 8
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr %result6)
  %13 = call i32 (ptr, ...) @printf(ptr @nl.14)
  ret i32 0
}

declare ptr @strcat(ptr, ptr)

declare i64 @strlen(ptr)

declare ptr @strcpy(ptr, ptr)

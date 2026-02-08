; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [13 x i8] c"Type of 42: \00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@typename = private unnamed_addr constant [4 x i8] c"int\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.3 = private unnamed_addr constant [18 x i8] c"Type of 'hello': \00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@typename.5 = private unnamed_addr constant [7 x i8] c"string\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %s = alloca ptr, align 8
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  store ptr @str, ptr %s, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.1)
  %x1 = load i32, ptr %x, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @typename)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.4, ptr @str.3)
  %s2 = load ptr, ptr %s, align 8
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @typename.5)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.7)
  ret i32 0
}

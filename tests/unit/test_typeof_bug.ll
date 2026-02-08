; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@typename = private unnamed_addr constant [7 x i8] c"string\00", align 1
@typename.1 = private unnamed_addr constant [4 x i8] c"int\00", align 1
@str.2 = private unnamed_addr constant [14 x i8] c"Type of text:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.4 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.5 = private unnamed_addr constant [13 x i8] c"Type of num:\00", align 1
@fmt.6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.7 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %num_type = alloca ptr, align 8
  %text_type = alloca ptr, align 8
  %num = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  store i32 42, ptr %num, align 4
  %text1 = load ptr, ptr %text, align 8
  store ptr @typename, ptr %text_type, align 8
  %num2 = load i32, ptr %num, align 4
  store ptr @typename.1, ptr %num_type, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.2)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %text_type3 = load ptr, ptr %text_type, align 8
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.3, ptr %text_type3)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.4)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.6, ptr @str.5)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.7)
  %num_type4 = load ptr, ptr %num_type, align 8
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr %num_type4)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  ret i32 0
}

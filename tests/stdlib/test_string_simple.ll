; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.1 = private unnamed_addr constant [10 x i8] c"  hello  \00", align 1
@str.2 = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@str.4 = private unnamed_addr constant [4 x i8] c"wyn\00", align 1
@str.5 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.6 = private unnamed_addr constant [6 x i8] c"HELLO\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %lower7 = alloca ptr, align 8
  %s5 = alloca ptr, align 8
  %upper5 = alloca ptr, align 8
  %s4 = alloca ptr, align 8
  %replaced = alloca ptr, align 8
  %s3 = alloca ptr, align 8
  %trimmed = alloca ptr, align 8
  %s2 = alloca ptr, align 8
  %sub = alloca ptr, align 8
  %s = alloca ptr, align 8
  store ptr @str, ptr %s, align 8
  %s1 = load ptr, ptr %s, align 8
  %substring = call ptr @wyn_substring(ptr %s1, i32 0, i32 5)
  store ptr %substring, ptr %sub, align 8
  store ptr @str.1, ptr %s2, align 8
  %s22 = load ptr, ptr %s2, align 8
  %trim = call ptr @wyn_trim(ptr %s22)
  store ptr %trim, ptr %trimmed, align 8
  store ptr @str.2, ptr %s3, align 8
  %s33 = load ptr, ptr %s3, align 8
  %replace = call ptr @wyn_replace(ptr %s33, ptr @str.3, ptr @str.4)
  store ptr %replace, ptr %replaced, align 8
  store ptr @str.5, ptr %s4, align 8
  %s44 = load ptr, ptr %s4, align 8
  %upper = call ptr @wyn_string_upper(ptr %s44)
  store ptr %upper, ptr %upper5, align 8
  store ptr @str.6, ptr %s5, align 8
  %s56 = load ptr, ptr %s5, align 8
  %lower = call ptr @wyn_string_lower(ptr %s56)
  store ptr %lower, ptr %lower7, align 8
  ret i32 0
}

declare ptr @wyn_substring(ptr, i32, i32)

declare ptr @wyn_trim(ptr)

declare ptr @wyn_replace(ptr, ptr, ptr)

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

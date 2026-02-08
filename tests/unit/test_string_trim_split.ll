; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [10 x i8] c"  hello  \00", align 1
@str.1 = private unnamed_addr constant [13 x i8] c"\\t\\ntest\\n\\t\00", align 1
@str.2 = private unnamed_addr constant [20 x i8] c"apple,banana,cherry\00", align 1
@str.3 = private unnamed_addr constant [14 x i8] c"usr/local/bin\00", align 1
@str.4 = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_trim() {
entry:
  %len2 = alloca i64, align 8
  %trimmed24 = alloca i32, align 4
  %text2 = alloca ptr, align 8
  %len = alloca i64, align 8
  %trimmed = alloca i32, align 4
  %text1 = alloca ptr, align 8
  store ptr @str, ptr %text1, align 8
  %text11 = load ptr, ptr %text1, align 8
  %trimmed2 = load i32, ptr %trimmed, align 4
  %strlen = call i64 @strlen(i32 %trimmed2)
  store i64 %strlen, ptr %len, align 4
  store ptr @str.1, ptr %text2, align 8
  %text23 = load ptr, ptr %text2, align 8
  %trimmed25 = load i32, ptr %trimmed24, align 4
  %strlen6 = call i64 @strlen(i32 %trimmed25)
  store i64 %strlen6, ptr %len2, align 4
  %len7 = load i64, ptr %len, align 4
  %len28 = load i64, ptr %len2, align 4
  %add = add i64 %len7, %len28
  ret i64 %add
}

define i32 @test_split() {
entry:
  %dir_count = alloca i64, align 8
  %dirs = alloca i32, align 4
  %path = alloca ptr, align 8
  %count = alloca i64, align 8
  %parts = alloca i32, align 4
  %csv = alloca ptr, align 8
  store ptr @str.2, ptr %csv, align 8
  %csv1 = load ptr, ptr %csv, align 8
  %parts2 = load i32, ptr %parts, align 4
  %strlen = call i64 @strlen(i32 %parts2)
  store i64 %strlen, ptr %count, align 4
  store ptr @str.3, ptr %path, align 8
  %path3 = load ptr, ptr %path, align 8
  %dirs4 = load i32, ptr %dirs, align 4
  %strlen5 = call i64 @strlen(i32 %dirs4)
  store i64 %strlen5, ptr %dir_count, align 4
  %count6 = load i64, ptr %count, align 4
  %dir_count7 = load i64, ptr %dir_count, align 4
  %add = add i64 %count6, %dir_count7
  ret i64 %add
}

define i32 @test_split_empty() {
entry:
  %count = alloca i64, align 8
  %parts = alloca i32, align 4
  %single = alloca ptr, align 8
  store ptr @str.4, ptr %single, align 8
  %single1 = load ptr, ptr %single, align 8
  %parts2 = load i32, ptr %parts, align 4
  %strlen = call i64 @strlen(i32 %parts2)
  store i64 %strlen, ptr %count, align 4
  %count3 = load i64, ptr %count, align 4
  ret i64 %count3
}

define i32 @wyn_main() {
entry:
  %empty_result = alloca i32, align 4
  %split_result = alloca i32, align 4
  %trim_result = alloca i32, align 4
  %test_trim = call i32 @test_trim()
  store i32 %test_trim, ptr %trim_result, align 4
  %test_split = call i32 @test_split()
  store i32 %test_split, ptr %split_result, align 4
  %test_split_empty = call i32 @test_split_empty()
  store i32 %test_split_empty, ptr %empty_result, align 4
  %trim_result1 = load i32, ptr %trim_result, align 4
  %split_result2 = load i32, ptr %split_result, align 4
  %add = add i32 %trim_result1, %split_result2
  %empty_result3 = load i32, ptr %empty_result, align 4
  %add4 = add i32 %add, %empty_result3
  ret i32 %add4
}

declare i64 @strlen(ptr)

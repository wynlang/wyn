; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.2 = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@str.3 = private unnamed_addr constant [2 x i8] c"a\00", align 1
@str.4 = private unnamed_addr constant [4 x i8] c"abc\00", align 1
@str.5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.6 = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@str.7 = private unnamed_addr constant [4 x i8] c"abc\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_pad_left() {
entry:
  %len2 = alloca i64, align 8
  %not_padded = alloca i32, align 4
  %already_long = alloca ptr, align 8
  %len = alloca i64, align 8
  %padded = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %padded2 = load i32, ptr %padded, align 4
  %strlen = call i64 @strlen(i32 %padded2)
  store i64 %strlen, ptr %len, align 4
  store ptr @str.1, ptr %already_long, align 8
  %already_long3 = load ptr, ptr %already_long, align 8
  %not_padded4 = load i32, ptr %not_padded, align 4
  %strlen5 = call i64 @strlen(i32 %not_padded4)
  store i64 %strlen5, ptr %len2, align 4
  %len6 = load i64, ptr %len, align 4
  %len27 = load i64, ptr %len2, align 4
  %add = add i64 %len6, %len27
  ret i64 %add
}

define i32 @test_pad_right() {
entry:
  %len2 = alloca i64, align 8
  %padded_x = alloca i32, align 4
  %with_x = alloca ptr, align 8
  %len = alloca i64, align 8
  %padded = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.2, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %padded2 = load i32, ptr %padded, align 4
  %strlen = call i64 @strlen(i32 %padded2)
  store i64 %strlen, ptr %len, align 4
  store ptr @str.3, ptr %with_x, align 8
  %with_x3 = load ptr, ptr %with_x, align 8
  %padded_x4 = load i32, ptr %padded_x, align 4
  %strlen5 = call i64 @strlen(i32 %padded_x4)
  store i64 %strlen5, ptr %len2, align 4
  %len6 = load i64, ptr %len, align 4
  %len27 = load i64, ptr %len2, align 4
  %add = add i64 %len6, %len27
  ret i64 %add
}

define i32 @test_chars() {
entry:
  %empty_count = alloca i64, align 8
  %empty_chars = alloca i32, align 4
  %empty = alloca ptr, align 8
  %count = alloca i64, align 8
  %char_array = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.4, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %char_array2 = load i32, ptr %char_array, align 4
  %strlen = call i64 @strlen(i32 %char_array2)
  store i64 %strlen, ptr %count, align 4
  store ptr @str.5, ptr %empty, align 8
  %empty3 = load ptr, ptr %empty, align 8
  %empty_chars4 = load i32, ptr %empty_chars, align 4
  %strlen5 = call i64 @strlen(i32 %empty_chars4)
  store i64 %strlen5, ptr %empty_count, align 4
  %count6 = load i64, ptr %count, align 4
  %empty_count7 = load i64, ptr %empty_count, align 4
  %add = add i64 %count6, %empty_count7
  ret i64 %add
}

define i32 @test_to_bytes() {
entry:
  %abc_count = alloca i64, align 8
  %abc_bytes = alloca i32, align 4
  %abc = alloca ptr, align 8
  %count = alloca i64, align 8
  %bytes = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.6, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %bytes2 = load i32, ptr %bytes, align 4
  %strlen = call i64 @strlen(i32 %bytes2)
  store i64 %strlen, ptr %count, align 4
  store ptr @str.7, ptr %abc, align 8
  %abc3 = load ptr, ptr %abc, align 8
  %abc_bytes4 = load i32, ptr %abc_bytes, align 4
  %strlen5 = call i64 @strlen(i32 %abc_bytes4)
  store i64 %strlen5, ptr %abc_count, align 4
  %count6 = load i64, ptr %count, align 4
  %abc_count7 = load i64, ptr %abc_count, align 4
  %add = add i64 %count6, %abc_count7
  ret i64 %add
}

define i32 @wyn_main() {
entry:
  %bytes_result = alloca i32, align 4
  %chars_result = alloca i32, align 4
  %pad_r = alloca i32, align 4
  %pad_l = alloca i32, align 4
  %test_pad_left = call i32 @test_pad_left()
  store i32 %test_pad_left, ptr %pad_l, align 4
  %test_pad_right = call i32 @test_pad_right()
  store i32 %test_pad_right, ptr %pad_r, align 4
  %test_chars = call i32 @test_chars()
  store i32 %test_chars, ptr %chars_result, align 4
  %test_to_bytes = call i32 @test_to_bytes()
  store i32 %test_to_bytes, ptr %bytes_result, align 4
  %pad_l1 = load i32, ptr %pad_l, align 4
  %pad_r2 = load i32, ptr %pad_r, align 4
  %add = add i32 %pad_l1, %pad_r2
  %chars_result3 = load i32, ptr %chars_result, align 4
  %add4 = add i32 %add, %chars_result3
  %bytes_result5 = load i32, ptr %bytes_result, align 4
  %add6 = add i32 %add4, %bytes_result5
  ret i32 %add6
}

declare i64 @strlen(ptr)

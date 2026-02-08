; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_array_len() {
entry:
  %len = alloca i64, align 8
  %arr = alloca ptr, align 8
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr4, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %strlen = call i64 @strlen(ptr %arr5)
  store i64 %strlen, ptr %len, align 4
  %len6 = load i64, ptr %len, align 4
  ret i64 %len6
}

define i32 @test_array_is_empty() {
entry:
  %empty = alloca i32, align 4
  %empty_arr = alloca ptr, align 8
  %not_empty = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr3 = load ptr, ptr %arr, align 8
  %array_literal4 = alloca [0 x i32], align 4
  store ptr %array_literal4, ptr %empty_arr, align 8
  %empty_arr5 = load ptr, ptr %empty_arr, align 8
  %not_empty6 = load i32, ptr %not_empty, align 4
  %tobool = icmp ne i32 %not_empty6, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @test_array_contains() {
entry:
  %has_10 = alloca i32, align 4
  %has_3 = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr4, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %strstr_result = call ptr @strstr(ptr %arr5, i32 3)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_3, align 4
  %arr6 = load ptr, ptr %arr, align 8
  %strstr_result7 = call ptr @strstr(ptr %arr6, i32 10)
  %is_found8 = icmp ne ptr %strstr_result7, null
  %contains9 = zext i1 %is_found8 to i32
  store i32 %contains9, ptr %has_10, align 4
  %has_310 = load i32, ptr %has_3, align 4
  %tobool = icmp ne i32 %has_310, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @test_string_vs_array_len() {
entry:
  %arr_len = alloca i64, align 8
  %arr = alloca ptr, align 8
  %text_len = alloca i64, align 8
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text1)
  store i64 %strlen, ptr %text_len, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %strlen5 = call i64 @strlen(ptr %arr4)
  store i64 %strlen5, ptr %arr_len, align 4
  %text_len6 = load i64, ptr %text_len, align 4
  %arr_len7 = load i64, ptr %arr_len, align 4
  %add = add i64 %text_len6, %arr_len7
  ret i64 %add
}

define i32 @wyn_main() {
entry:
  %combined = alloca i32, align 4
  %contains = alloca i32, align 4
  %empty = alloca i32, align 4
  %len = alloca i32, align 4
  %test_array_len = call i32 @test_array_len()
  store i32 %test_array_len, ptr %len, align 4
  %test_array_is_empty = call i32 @test_array_is_empty()
  store i32 %test_array_is_empty, ptr %empty, align 4
  %test_array_contains = call i32 @test_array_contains()
  store i32 %test_array_contains, ptr %contains, align 4
  %test_string_vs_array_len = call i32 @test_string_vs_array_len()
  store i32 %test_string_vs_array_len, ptr %combined, align 4
  %len1 = load i32, ptr %len, align 4
  %empty2 = load i32, ptr %empty, align 4
  %add = add i32 %len1, %empty2
  %contains3 = load i32, ptr %contains, align 4
  %add4 = add i32 %add, %contains3
  %combined5 = load i32, ptr %combined, align 4
  %add6 = add i32 %add4, %combined5
  ret i32 %add6
}

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

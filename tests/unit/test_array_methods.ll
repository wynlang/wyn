; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_array_push() {
entry:
  %len = alloca i64, align 8
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
  %arr4 = load ptr, ptr %arr, align 8
  %strlen = call i64 @strlen(ptr %arr4)
  store i64 %strlen, ptr %len, align 4
  %len5 = load i64, ptr %len, align 4
  ret i64 %len5
}

define i32 @test_array_pop() {
entry:
  %len = alloca i64, align 8
  %last = alloca i32, align 4
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
  %arr4 = load ptr, ptr %arr, align 8
  %strlen = call i64 @strlen(ptr %arr4)
  store i64 %strlen, ptr %len, align 4
  %last5 = load i32, ptr %last, align 4
  %len6 = load i64, ptr %len, align 4
  %add = add i32 %last5, i64 %len6
  ret i32 %add
}

define i32 @test_array_get() {
entry:
  %third = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [4 x i32], align 4
  %element_ptr = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 0
  store i32 10, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 1
  store i32 20, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 2
  store i32 30, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 3
  store i32 40, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %first6 = load i32, ptr %first, align 4
  %third7 = load i32, ptr %third, align 4
  %add = add i32 %first6, %third7
  ret i32 %add
}

define i32 @test_array_index_of() {
entry:
  %not_found = alloca i32, align 4
  %idx = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [4 x i32], align 4
  %element_ptr = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 0
  store i32 5, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 1
  store i32 10, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 2
  store i32 15, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 3
  store i32 20, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %idx6 = load i32, ptr %idx, align 4
  %not_found7 = load i32, ptr %not_found, align 4
  %add = add i32 %idx6, %not_found7
  ret i32 %add
}

define i32 @test_array_reverse() {
entry:
  %last = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [4 x i32], align 4
  %element_ptr = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %arr6 = load ptr, ptr %arr, align 8
  %first7 = load i32, ptr %first, align 4
  %last8 = load i32, ptr %last, align 4
  %add = add i32 %first7, %last8
  ret i32 %add
}

define i32 @test_array_sort() {
entry:
  %last = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [4 x i32], align 4
  %element_ptr = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 0
  store i32 3, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 1
  store i32 1, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 2
  store i32 4, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [4 x i32], ptr %array_literal, i32 0, i32 3
  store i32 2, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %arr6 = load ptr, ptr %arr, align 8
  %first7 = load i32, ptr %first, align 4
  %last8 = load i32, ptr %last, align 4
  %add = add i32 %first7, %last8
  ret i32 %add
}

define i32 @wyn_main() {
entry:
  %sort_result = alloca i32, align 4
  %reverse_result = alloca i32, align 4
  %index_result = alloca i32, align 4
  %get_result = alloca i32, align 4
  %pop_result = alloca i32, align 4
  %push_result = alloca i32, align 4
  %test_array_push = call i32 @test_array_push()
  store i32 %test_array_push, ptr %push_result, align 4
  %test_array_pop = call i32 @test_array_pop()
  store i32 %test_array_pop, ptr %pop_result, align 4
  %test_array_get = call i32 @test_array_get()
  store i32 %test_array_get, ptr %get_result, align 4
  %test_array_index_of = call i32 @test_array_index_of()
  store i32 %test_array_index_of, ptr %index_result, align 4
  %test_array_reverse = call i32 @test_array_reverse()
  store i32 %test_array_reverse, ptr %reverse_result, align 4
  %test_array_sort = call i32 @test_array_sort()
  store i32 %test_array_sort, ptr %sort_result, align 4
  %push_result1 = load i32, ptr %push_result, align 4
  %pop_result2 = load i32, ptr %pop_result, align 4
  %add = add i32 %push_result1, %pop_result2
  %get_result3 = load i32, ptr %get_result, align 4
  %add4 = add i32 %add, %get_result3
  %index_result5 = load i32, ptr %index_result, align 4
  %add6 = add i32 %add4, %index_result5
  %reverse_result7 = load i32, ptr %reverse_result, align 4
  %add8 = add i32 %add6, %reverse_result7
  %sort_result9 = load i32, ptr %sort_result, align 4
  %add10 = add i32 %add8, %sort_result9
  ret i32 %add10
}

declare i64 @strlen(ptr)

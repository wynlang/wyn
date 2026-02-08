; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.2 = private unnamed_addr constant [10 x i8] c"  HELLO  \00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_string_methods() {
entry:
  %title = alloca i32, align 4
  %contains5 = alloca i32, align 4
  %len = alloca i64, align 8
  %upper2 = alloca ptr, align 8
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store ptr %upper, ptr %upper2, align 8
  %text3 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text3)
  store i64 %strlen, ptr %len, align 4
  %text4 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text4, ptr @str.1)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %contains5, align 4
  %text6 = load ptr, ptr %text, align 8
  %len7 = load i64, ptr %len, align 4
  ret i64 %len7
}

define i32 @test_number_methods() {
entry:
  %floored = alloca i32, align 4
  %rounded = alloca i32, align 4
  %flt = alloca double, align 8
  %even = alloca i32, align 4
  %str = alloca ptr, align 8
  %num = alloca i32, align 4
  store i32 42, ptr %num, align 4
  %num1 = load i32, ptr %num, align 4
  %buffer = call ptr @malloc(i64 32)
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt, i32 %num1)
  store ptr %buffer, ptr %str, align 8
  %num2 = load i32, ptr %num, align 4
  store double 3.140000e+00, ptr %flt, align 8
  %flt3 = load double, ptr %flt, align 8
  %flt4 = load double, ptr %flt, align 8
  %num5 = load i32, ptr %num, align 4
  ret i32 %num5
}

define i32 @test_array_methods() {
entry:
  %last = alloca i32, align 4
  %first = alloca i32, align 4
  %len = alloca i64, align 8
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
  %strlen = call i64 @strlen(ptr %arr4)
  store i64 %strlen, ptr %len, align 4
  %arr5 = load ptr, ptr %arr, align 8
  %arr6 = load ptr, ptr %arr, align 8
  %arr7 = load ptr, ptr %arr, align 8
  %arr8 = load ptr, ptr %arr, align 8
  %first9 = load i32, ptr %first, align 4
  %last10 = load i32, ptr %last, align 4
  %add = add i32 %first9, %last10
  ret i32 %add
}

define i32 @test_method_chaining() {
entry:
  %int_result = alloca i32, align 4
  %num = alloca double, align 8
  %result = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.2, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store double 3.700000e+00, ptr %num, align 8
  %num2 = load double, ptr %num, align 8
  %int_result3 = load i32, ptr %int_result, align 4
  ret i32 %int_result3
}

define i32 @test_type_aware_dispatch() {
entry:
  %arr_len = alloca i64, align 8
  %arr = alloca ptr, align 8
  %text_len = alloca i64, align 8
  %text = alloca ptr, align 8
  store ptr @str.3, ptr %text, align 8
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
  %d = alloca i32, align 4
  %c = alloca i32, align 4
  %a = alloca i32, align 4
  %n = alloca i32, align 4
  %s = alloca i32, align 4
  %test_string_methods = call i32 @test_string_methods()
  store i32 %test_string_methods, ptr %s, align 4
  %test_number_methods = call i32 @test_number_methods()
  store i32 %test_number_methods, ptr %n, align 4
  %test_array_methods = call i32 @test_array_methods()
  store i32 %test_array_methods, ptr %a, align 4
  %test_method_chaining = call i32 @test_method_chaining()
  store i32 %test_method_chaining, ptr %c, align 4
  %test_type_aware_dispatch = call i32 @test_type_aware_dispatch()
  store i32 %test_type_aware_dispatch, ptr %d, align 4
  %s1 = load i32, ptr %s, align 4
  %n2 = load i32, ptr %n, align 4
  %add = add i32 %s1, %n2
  %a3 = load i32, ptr %a, align 4
  %add4 = add i32 %add, %a3
  %c5 = load i32, ptr %c, align 4
  %add6 = add i32 %add4, %c5
  %d7 = load i32, ptr %d, align 4
  %add8 = add i32 %add6, %d7
  ret i32 %add8
}

declare ptr @wyn_string_upper(ptr)

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

declare i32 @sprintf(ptr, ptr, ...)

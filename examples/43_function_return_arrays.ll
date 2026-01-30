; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Alice\00", align 1
@str.1 = private unnamed_addr constant [4 x i8] c"Bob\00", align 1
@str.2 = private unnamed_addr constant [8 x i8] c"Charlie\00", align 1
@str.3 = private unnamed_addr constant [16 x i8] c"Numbers count: \00", align 1
@str.4 = private unnamed_addr constant [13 x i8] c"First name: \00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @get_numbers() {
entry:
  %result = alloca ptr, align 8
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
  store ptr %array_literal, ptr %result, align 8
  %result5 = load ptr, ptr %result, align 8
  ret ptr %result5
}

define i32 @get_names() {
entry:
  %names = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store ptr @str, ptr %element_ptr, align 8
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store ptr @str.1, ptr %element_ptr1, align 8
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store ptr @str.2, ptr %element_ptr2, align 8
  store ptr %array_literal, ptr %names, align 8
  %names3 = load ptr, ptr %names, align 8
  ret ptr %names3
}

define i32 @wyn_main() {
entry:
  %first = alloca i32, align 4
  %names = alloca i32, align 4
  %nums = alloca i32, align 4
  %get_numbers = call i32 @get_numbers()
  store i32 %get_numbers, ptr %nums, align 4
  %get_names = call i32 @get_names()
  store i32 %get_names, ptr %names, align 4
  %first1 = load i32, ptr %first, align 4
  %add = add ptr @str.4, i32 %first1
  %print_string_call = call void @print_string(ptr %add)
  ret i32 0
}

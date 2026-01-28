; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [16 x i8] c"  Hello, Wyn!  \00", align 1
@str.1 = private unnamed_addr constant [3 x i8] c"42\00", align 1
@str.2 = private unnamed_addr constant [20 x i8] c"apple,banana,cherry\00", align 1
@str.3 = private unnamed_addr constant [17 x i8] c"user@example.com\00", align 1
@str.4 = private unnamed_addr constant [3 x i8] c"42\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %min_val = alloca i32, align 4
  %max_val = alloca i32, align 4
  %abs_val = alloca i32, align 4
  %input = alloca i32, align 4
  %data = alloca i32, align 4
  %first_char = alloca i32, align 4
  %username = alloca i32, align 4
  %at_pos = alloca i32, align 4
  %email = alloca i32, align 4
  %fruit_count = alloca i32, align 4
  %first_fruit = alloca i32, align 4
  %csv = alloca i32, align 4
  %str = alloca i32, align 4
  %num = alloca i32, align 4
  %num_str = alloca i32, align 4
  %has_urgent = alloca i32, align 4
  %tags = alloca i32, align 4
  %has_bob = alloca i32, align 4
  %alice_score = alloca i32, align 4
  %scores = alloca i32, align 4
  %has_three = alloca i32, align 4
  %nums = alloca i32, align 4
  %len = alloca i32, align 4
  %upper = alloca i32, align 4
  %trimmed = alloca i32, align 4
  %text = alloca i32, align 4
  store ptr @str, ptr %text, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %nums, align 8
  store ptr @str.1, ptr %num_str, align 8
  store ptr @str.2, ptr %csv, align 8
  store ptr @str.3, ptr %email, align 8
  %array_literal3 = alloca [4 x i32], align 4
  %element_ptr4 = getelementptr [4 x i32], ptr %array_literal3, i32 0, i32 0
  store i32 5, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [4 x i32], ptr %array_literal3, i32 0, i32 1
  store i32 10, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [4 x i32], ptr %array_literal3, i32 0, i32 2
  store i32 15, ptr %element_ptr6, align 4
  %element_ptr7 = getelementptr [4 x i32], ptr %array_literal3, i32 0, i32 3
  store i32 20, ptr %element_ptr7, align 4
  store ptr %array_literal3, ptr %data, align 8
  store ptr @str.4, ptr %input, align 8
  ret i32 0
}

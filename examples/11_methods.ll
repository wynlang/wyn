; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"hello world\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %has_urgent = alloca i32, align 4
  %tags = alloca i32, align 4
  %scores = alloca i32, align 4
  %has_three = alloca i32, align 4
  %arr_len = alloca i32, align 4
  %arr = alloca i32, align 4
  %abs_val = alloca i32, align 4
  %num = alloca i32, align 4
  %len = alloca i32, align 4
  %upper = alloca i32, align 4
  %text = alloca i32, align 4
  store ptr @str, ptr %text, align 8
  store i32 42, ptr %num, align 4
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
  ret i32 0
}

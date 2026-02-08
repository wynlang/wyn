; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %final_value = alloca i32, align 4
  %result = alloca i32, align 4
  %selected = alloca i32, align 4
  %values = alloca ptr, align 8
  %area = alloca i32, align 4
  store i32 200, ptr %area, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 100, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 200, ptr %element_ptr1, align 4
  %area2 = load i32, ptr %area, align 4
  %element_ptr3 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 %area2, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %values, align 8
  %values4 = load ptr, ptr %values, align 8
  %array_element_ptr = getelementptr [0 x i32], ptr %values4, i32 0, i32 2
  %array_element = load i32, ptr %array_element_ptr, align 4
  store i32 %array_element, ptr %selected, align 4
  %result5 = load i32, ptr %result, align 4
  store i32 %result5, ptr %final_value, align 4
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @add_vec2(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  ret i32 0
}

define i32 @magnitude(i32 %v) {
entry:
  %v1 = alloca i32, align 4
  store i32 %v, ptr %v1, align 4
  ret i32 1
}

define i32 @process_array(i32 %arr) {
entry:
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  %arr2 = load i32, ptr %arr1, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr2, 0, 0
  %array_element = load i32, i32 %array_element_ptr, align 4
  ret i32 %array_element
}

define i32 @wyn_main() {
entry:
  %mag = alloca i32, align 4
  %result = alloca i32, align 4
  %arr = alloca ptr, align 8
  %v3 = alloca i32, align 4
  %v2 = alloca i32, align 4
  %v1 = alloca i32, align 4
  %v11 = load i32, ptr %v1, align 4
  %v22 = load i32, ptr %v2, align 4
  %add_vec2 = call i32 @add_vec2(i32 %v11, i32 %v22)
  store i32 %add_vec2, ptr %v3, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 100, ptr %element_ptr, align 4
  store ptr %array_literal, ptr %arr, align 8
  %v33 = load i32, ptr %v3, align 4
  %magnitude = call i32 @magnitude(i32 %v33)
  store i32 %magnitude, ptr %mag, align 4
  ret i32 0
}

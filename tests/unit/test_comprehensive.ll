; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %arr = alloca ptr, align 8
  %p = alloca i32, align 4
  %x = alloca i32, align 4
  %add = call i32 @add(i32 10, i32 20)
  store i32 %add, ptr %x, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %result3 = load i32, ptr %result, align 4
  ret i32 %result3
}

define i32 @add(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %add = add i32 %a3, %b4
  ret i32 %add
}

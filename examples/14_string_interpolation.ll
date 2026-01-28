; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Alice\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.2 = private unnamed_addr constant [21 x i8] c"  USER@EXAMPLE.COM  \00", align 1
@str.3 = private unnamed_addr constant [14 x i8] c"/tmp/test.txt\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %timestamp = alloca i32, align 4
  %cwd = alloca i32, align 4
  %test_file = alloca i32, align 4
  %cleaned = alloca i32, align 4
  %email = alloca i32, align 4
  %total_sum = alloca i32, align 4
  %len = alloca i32, align 4
  %numbers = alloca i32, align 4
  %is_ready = alloca i32, align 4
  %is_valid = alloca i32, align 4
  %total = alloca i32, align 4
  %quantity = alloca i32, align 4
  %price = alloca i32, align 4
  %upper_text = alloca i32, align 4
  %text = alloca i32, align 4
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %age = alloca i32, align 4
  %name = alloca i32, align 4
  store ptr @str, ptr %name, align 8
  store i32 25, ptr %age, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store ptr @str.1, ptr %text, align 8
  store i32 100, ptr %price, align 4
  store i32 3, ptr %quantity, align 4
  store i1 true, ptr %is_valid, align 1
  store i1 false, ptr %is_ready, align 1
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
  store ptr %array_literal, ptr %numbers, align 8
  store ptr @str.2, ptr %email, align 8
  store ptr @str.3, ptr %test_file, align 8
  ret i32 0
}

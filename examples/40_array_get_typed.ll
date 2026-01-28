; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Alice\00", align 1
@str.1 = private unnamed_addr constant [4 x i8] c"Bob\00", align 1
@str.2 = private unnamed_addr constant [8 x i8] c"Charlie\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"Alice\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %sum = alloca i32, align 4
  %num2 = alloca i32, align 4
  %num1 = alloca i32, align 4
  %numbers = alloca i32, align 4
  %second = alloca i32, align 4
  %first = alloca i32, align 4
  %names = alloca i32, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store ptr @str, ptr %element_ptr, align 8
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store ptr @str.1, ptr %element_ptr1, align 8
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store ptr @str.2, ptr %element_ptr2, align 8
  store ptr %array_literal, ptr %names, align 8
  %array_literal3 = alloca [3 x i32], align 4
  %element_ptr4 = getelementptr [3 x i32], ptr %array_literal3, i32 0, i32 0
  store i32 10, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [3 x i32], ptr %array_literal3, i32 0, i32 1
  store i32 20, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [3 x i32], ptr %array_literal3, i32 0, i32 2
  store i32 30, ptr %element_ptr6, align 4
  store ptr %array_literal3, ptr %numbers, align 8
  ret i32 0
}

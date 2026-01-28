; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"Alice\00", align 1
@str.1 = private unnamed_addr constant [4 x i8] c"Bob\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %first = alloca i32, align 4
  %names = alloca i32, align 4
  %array_literal = alloca [2 x i32], align 4
  %element_ptr = getelementptr [2 x i32], ptr %array_literal, i32 0, i32 0
  store ptr @str, ptr %element_ptr, align 8
  %element_ptr1 = getelementptr [2 x i32], ptr %array_literal, i32 0, i32 1
  store ptr @str.1, ptr %element_ptr1, align 8
  store ptr %array_literal, ptr %names, align 8
  ret i32 0
}

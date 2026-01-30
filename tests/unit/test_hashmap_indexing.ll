; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [2 x i8] c"a\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.1 = private unnamed_addr constant [2 x i8] c"d\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %val = alloca i32, align 4
  %map = alloca i32, align 4
  %map1 = load i32, ptr %map, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %map1, i32 0, ptr @str
  %array_element = load i32, i32 %array_element_ptr, align 4
  store i32 %array_element, ptr %val, align 4
  %val2 = load i32, ptr %val, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 %val2)
  %map3 = load i32, ptr %map, align 4
  %array_element_ptr4 = getelementptr [0 x i32], i32 %map3, i32 0, ptr @str.1
  %array_element5 = load i32, i32 %array_element_ptr4, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 %array_element5)
  ret i32 0
}

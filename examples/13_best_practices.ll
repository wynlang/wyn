; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [16 x i8] c"  hello world  \00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"page3\00", align 1
@str.2 = private unnamed_addr constant [14 x i8] c"name,age,city\00", align 1
@str.3 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %input = alloca i32, align 4
  %data = alloca i32, align 4
  %items = alloca i32, align 4
  %field3 = alloca i32, align 4
  %field2 = alloca i32, align 4
  %field1 = alloca i32, align 4
  %csv = alloca i32, align 4
  %page = alloca i32, align 4
  %visited = alloca i32, align 4
  %config = alloca i32, align 4
  %result = alloca i32, align 4
  %text = alloca i32, align 4
  store ptr @str, ptr %text, align 8
  store ptr @str.1, ptr %page, align 8
  store ptr @str.2, ptr %csv, align 8
  %array_literal = alloca [0 x i32], align 4
  store ptr %array_literal, ptr %items, align 8
  store ptr @str.3, ptr %input, align 8
  ret i32 0
}

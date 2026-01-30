; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [10 x i8] c"  HELLO  \00", align 1
@str.2 = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.3 = private unnamed_addr constant [11 x i8] c"  spaces  \00", align 1
@str.4 = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_string_method_inference() {
entry:
  %upper2 = alloca ptr, align 8
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store ptr %upper, ptr %upper2, align 8
  ret i32 0
}

define i32 @test_string_method_chaining() {
entry:
  %result = alloca i32, align 4
  ret i32 0
}

define i32 @test_multiple_string_methods() {
entry:
  %trimmed = alloca i32, align 4
  %lower4 = alloca ptr, align 8
  %upper2 = alloca ptr, align 8
  %text = alloca ptr, align 8
  store ptr @str.2, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store ptr %upper, ptr %upper2, align 8
  %text3 = load ptr, ptr %text, align 8
  %lower = call ptr @wyn_string_lower(ptr %text3)
  store ptr %lower, ptr %lower4, align 8
  ret i32 0
}

define i32 @test_mixed_types() {
entry:
  %num = alloca i32, align 4
  %upper2 = alloca ptr, align 8
  %text = alloca ptr, align 8
  store ptr @str.4, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  store ptr %upper, ptr %upper2, align 8
  store i32 42, ptr %num, align 4
  %num3 = load i32, ptr %num, align 4
  ret i32 %num3
}

define i32 @wyn_main() {
entry:
  %exit_code = alloca i32, align 4
  %test_string_method_inference = call i32 @test_string_method_inference()
  %test_string_method_chaining = call i32 @test_string_method_chaining()
  %test_multiple_string_methods = call i32 @test_multiple_string_methods()
  %test_mixed_types = call i32 @test_mixed_types()
  store i32 %test_mixed_types, ptr %exit_code, align 4
  %exit_code1 = load i32, ptr %exit_code, align 4
  ret i32 %exit_code1
}

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

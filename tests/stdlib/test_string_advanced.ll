; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [18 x i8] c"hello world hello\00", align 1
@str.1 = private unnamed_addr constant [4 x i8] c"123\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"12.34\00", align 1
@str.3 = private unnamed_addr constant [4 x i8] c"abc\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %not_num = alloca ptr, align 8
  %num2 = alloca ptr, align 8
  %num1 = alloca ptr, align 8
  %count = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %count2 = load i32, ptr %count, align 4
  %icmp = icmp ne i32 %count2, 2
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  store ptr @str.1, ptr %num1, align 8
  %num13 = load ptr, ptr %num1, align 8
  store ptr @str.2, ptr %num2, align 8
  %num24 = load ptr, ptr %num2, align 8
  store ptr @str.3, ptr %not_num, align 8
  %not_num5 = load ptr, ptr %not_num, align 8
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %l = alloca i32, align 4
  %f = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr3 = load ptr, ptr %arr, align 8
  %f4 = load i32, ptr %f, align 4
  %icmp = icmp ne i32 %f4, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %arr5 = load ptr, ptr %arr, align 8
  %l6 = load i32, ptr %l, align 4
  %icmp7 = icmp ne i32 %l6, 3
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end
  ret i32 2

if.end9:                                          ; preds = %if.end
  ret i32 0
}

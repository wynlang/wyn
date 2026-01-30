; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"\\nSpawning \00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c" tasks...\\n\00", align 1
@str.2 = private unnamed_addr constant [8 x i8] c"Done!\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @empty_worker() {
entry:
  %x = alloca i32, align 4
  store i32 1, ptr %x, align 4
  ret void
}

define void @wyn_main() {
entry:
  %j = alloca i32, align 4
  %count = alloca i32, align 4
  %i = alloca i32, align 4
  %counts = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1000, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 10000, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 100000, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %counts, align 8
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.end10, %entry
  %i3 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i3, 3
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counts4 = load ptr, ptr %counts, align 8
  %i5 = load i32, ptr %i, align 4
  %array_element_ptr = getelementptr [0 x i32], ptr %counts4, i32 0, i32 %i5
  %array_element = load i32, ptr %array_element_ptr, align 4
  store i32 %array_element, ptr %count, align 4
  %print_string_call = call void @print_string(ptr @str)
  %count6 = load i32, ptr %count, align 4
  %print_call = call void @print(i32 %count6)
  %print_string_call7 = call void @print_string(ptr @str.1)
  store i32 0, ptr %j, align 4
  br label %while.header8

while.end:                                        ; preds = %while.header
  ret i32 0

while.header8:                                    ; preds = %while.body9, %while.body
  %j11 = load i32, ptr %j, align 4
  %count12 = load i32, ptr %count, align 4
  %icmp13 = icmp slt i32 %j11, %count12
  br i1 %icmp13, label %while.body9, label %while.end10

while.body9:                                      ; preds = %while.header8
  %j14 = load i32, ptr %j, align 4
  %add = add i32 %j14, 1
  store i32 %add, ptr %j, align 4
  br label %while.header8

while.end10:                                      ; preds = %while.header8
  %print_string_call15 = call void @print_string(ptr @str.2)
  %i16 = load i32, ptr %i, align 4
  %add17 = add i32 %i16, 1
  store i32 %add17, ptr %i, align 4
  br label %while.header
}

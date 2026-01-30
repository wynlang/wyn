; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @sum_array(i32 %arr, i32 %len) {
entry:
  %i = alloca i32, align 4
  %total = alloca i32, align 4
  %len2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %len, ptr %len2, align 4
  store i32 0, ptr %total, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %len4 = load i32, ptr %len2, align 4
  %icmp = icmp slt i32 %i3, %len4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %total5 = load i32, ptr %total, align 4
  %arr6 = load i32, ptr %arr1, align 4
  %i7 = load i32, ptr %i, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr6, 0, %i7
  %array_element = load i32, i32 %array_element_ptr, align 4
  %add = add i32 %total5, %array_element
  store i32 %add, ptr %total, align 4
  %i8 = load i32, ptr %i, align 4
  %add9 = add i32 %i8, 1
  store i32 %add9, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %total10 = load i32, ptr %total, align 4
  ret i32 %total10
}

define i32 @max_array(i32 %arr, i32 %len) {
entry:
  %i = alloca i32, align 4
  %maximum = alloca i32, align 4
  %len2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %len, ptr %len2, align 4
  %arr3 = load i32, ptr %arr1, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr3, 0, 0
  %array_element = load i32, i32 %array_element_ptr, align 4
  store i32 %array_element, ptr %maximum, align 4
  store i32 1, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %if.end, %entry
  %i4 = load i32, ptr %i, align 4
  %len5 = load i32, ptr %len2, align 4
  %icmp = icmp slt i32 %i4, %len5
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %arr6 = load i32, ptr %arr1, align 4
  %i7 = load i32, ptr %i, align 4
  %array_element_ptr8 = getelementptr [0 x i32], i32 %arr6, 0, %i7
  %array_element9 = load i32, i32 %array_element_ptr8, align 4
  %maximum10 = load i32, ptr %maximum, align 4
  %icmp11 = icmp sgt i32 %array_element9, %maximum10
  br i1 %icmp11, label %if.then, label %if.end

while.end:                                        ; preds = %while.header
  %maximum17 = load i32, ptr %maximum, align 4
  ret i32 %maximum17

if.then:                                          ; preds = %while.body
  %arr12 = load i32, ptr %arr1, align 4
  %i13 = load i32, ptr %i, align 4
  %array_element_ptr14 = getelementptr [0 x i32], i32 %arr12, 0, %i13
  %array_element15 = load i32, i32 %array_element_ptr14, align 4
  store i32 %array_element15, ptr %maximum, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %i16 = load i32, ptr %i, align 4
  %add = add i32 %i16, 1
  store i32 %add, ptr %i, align 4
  br label %while.header
}

define i32 @wyn_main() {
entry:
  %max = alloca i32, align 4
  %sum = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %array_literal = alloca [6 x i32], align 4
  %element_ptr = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 0
  store i32 5, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 2
  store i32 8, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 3
  store i32 1, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 4
  store i32 9, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 5
  store i32 3, ptr %element_ptr5, align 4
  store ptr %array_literal, ptr %numbers, align 8
  %numbers6 = load ptr, ptr %numbers, align 8
  %sum_array = call i32 @sum_array(ptr %numbers6, i32 6)
  store i32 %sum_array, ptr %sum, align 4
  %numbers7 = load ptr, ptr %numbers, align 8
  %max_array = call i32 @max_array(ptr %numbers7, i32 6)
  store i32 %max_array, ptr %max, align 4
  %sum8 = load i32, ptr %sum, align 4
  %max9 = load i32, ptr %max, align 4
  %add = add i32 %sum8, %max9
  ret i32 %add
}

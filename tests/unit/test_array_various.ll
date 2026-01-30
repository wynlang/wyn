; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @array_sum_test(i32 %arr, i32 %len) {
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

define i32 @array_max_test(i32 %arr, i32 %len) {
entry:
  %i = alloca i32, align 4
  %maximum = alloca i32, align 4
  %len2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %len, ptr %len2, align 4
  %len3 = load i32, ptr %len2, align 4
  %icmp = icmp eq i32 %len3, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 0

if.end:                                           ; preds = %entry
  %arr4 = load i32, ptr %arr1, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr4, 0, 0
  %array_element = load i32, i32 %array_element_ptr, align 4
  store i32 %array_element, ptr %maximum, align 4
  store i32 1, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %if.end15, %if.end
  %i5 = load i32, ptr %i, align 4
  %len6 = load i32, ptr %len2, align 4
  %icmp7 = icmp slt i32 %i5, %len6
  br i1 %icmp7, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %arr8 = load i32, ptr %arr1, align 4
  %i9 = load i32, ptr %i, align 4
  %array_element_ptr10 = getelementptr [0 x i32], i32 %arr8, 0, %i9
  %array_element11 = load i32, i32 %array_element_ptr10, align 4
  %maximum12 = load i32, ptr %maximum, align 4
  %icmp13 = icmp sgt i32 %array_element11, %maximum12
  br i1 %icmp13, label %if.then14, label %if.end15

while.end:                                        ; preds = %while.header
  %maximum21 = load i32, ptr %maximum, align 4
  ret i32 %maximum21

if.then14:                                        ; preds = %while.body
  %arr16 = load i32, ptr %arr1, align 4
  %i17 = load i32, ptr %i, align 4
  %array_element_ptr18 = getelementptr [0 x i32], i32 %arr16, 0, %i17
  %array_element19 = load i32, i32 %array_element_ptr18, align 4
  store i32 %array_element19, ptr %maximum, align 4
  br label %if.end15

if.end15:                                         ; preds = %if.then14, %while.body
  %i20 = load i32, ptr %i, align 4
  %add = add i32 %i20, 1
  store i32 %add, ptr %i, align 4
  br label %while.header
}

define i32 @wyn_main() {
entry:
  %m2 = alloca i32, align 4
  %s2 = alloca i32, align 4
  %arr2 = alloca ptr, align 8
  %m1 = alloca i32, align 4
  %s1 = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 10, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 20, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 30, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr1, align 8
  %arr13 = load ptr, ptr %arr1, align 8
  %array_sum_test = call i32 @array_sum_test(ptr %arr13, i32 3)
  store i32 %array_sum_test, ptr %s1, align 4
  %arr14 = load ptr, ptr %arr1, align 8
  %array_max_test = call i32 @array_max_test(ptr %arr14, i32 3)
  store i32 %array_max_test, ptr %m1, align 4
  %array_literal5 = alloca [4 x i32], align 4
  %element_ptr6 = getelementptr [4 x i32], ptr %array_literal5, i32 0, i32 0
  store i32 5, ptr %element_ptr6, align 4
  %element_ptr7 = getelementptr [4 x i32], ptr %array_literal5, i32 0, i32 1
  store i32 15, ptr %element_ptr7, align 4
  %element_ptr8 = getelementptr [4 x i32], ptr %array_literal5, i32 0, i32 2
  store i32 25, ptr %element_ptr8, align 4
  %element_ptr9 = getelementptr [4 x i32], ptr %array_literal5, i32 0, i32 3
  store i32 35, ptr %element_ptr9, align 4
  store ptr %array_literal5, ptr %arr2, align 8
  %arr210 = load ptr, ptr %arr2, align 8
  %array_sum_test11 = call i32 @array_sum_test(ptr %arr210, i32 4)
  store i32 %array_sum_test11, ptr %s2, align 4
  %arr212 = load ptr, ptr %arr2, align 8
  %array_max_test13 = call i32 @array_max_test(ptr %arr212, i32 4)
  store i32 %array_max_test13, ptr %m2, align 4
  %s114 = load i32, ptr %s1, align 4
  %m115 = load i32, ptr %m1, align 4
  %add = add i32 %s114, %m115
  %s216 = load i32, ptr %s2, align 4
  %add17 = add i32 %add, %s216
  %m218 = load i32, ptr %m2, align 4
  %add19 = add i32 %add17, %m218
  ret i32 %add19
}

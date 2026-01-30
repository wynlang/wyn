; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @binary_search(i32 %arr, i32 %len, i32 %target) {
entry:
  %mid = alloca i32, align 4
  %right = alloca i32, align 4
  %left = alloca i32, align 4
  %target3 = alloca i32, align 4
  %len2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %len, ptr %len2, align 4
  store i32 %target, ptr %target3, align 4
  store i32 0, ptr %left, align 4
  %len4 = load i32, ptr %len2, align 4
  %sub = sub i32 %len4, 1
  store i32 %sub, ptr %right, align 4
  br label %while.header

while.header:                                     ; preds = %if.end21, %entry
  %left5 = load i32, ptr %left, align 4
  %right6 = load i32, ptr %right, align 4
  %icmp = icmp sle i32 %left5, %right6
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %left7 = load i32, ptr %left, align 4
  %right8 = load i32, ptr %right, align 4
  %add = add i32 %left7, %right8
  %div = sdiv i32 %add, 2
  store i32 %div, ptr %mid, align 4
  %arr9 = load i32, ptr %arr1, align 4
  %mid10 = load i32, ptr %mid, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr9, 0, %mid10
  %array_element = load i32, i32 %array_element_ptr, align 4
  %target11 = load i32, ptr %target3, align 4
  %icmp12 = icmp eq i32 %array_element, %target11
  br i1 %icmp12, label %if.then, label %if.end

while.end:                                        ; preds = %while.header
  ret i32 -1

if.then:                                          ; preds = %while.body
  %mid13 = load i32, ptr %mid, align 4
  ret i32 %mid13

if.end:                                           ; preds = %while.body
  %arr14 = load i32, ptr %arr1, align 4
  %mid15 = load i32, ptr %mid, align 4
  %array_element_ptr16 = getelementptr [0 x i32], i32 %arr14, 0, %mid15
  %array_element17 = load i32, i32 %array_element_ptr16, align 4
  %target18 = load i32, ptr %target3, align 4
  %icmp19 = icmp slt i32 %array_element17, %target18
  br i1 %icmp19, label %if.then20, label %if.else

if.then20:                                        ; preds = %if.end
  %mid22 = load i32, ptr %mid, align 4
  %add23 = add i32 %mid22, 1
  store i32 %add23, ptr %left, align 4
  br label %if.end21

if.else:                                          ; preds = %if.end
  %mid24 = load i32, ptr %mid, align 4
  %sub25 = sub i32 %mid24, 1
  store i32 %sub25, ptr %right, align 4
  br label %if.end21

if.end21:                                         ; preds = %if.else, %if.then20
  br label %while.header
}

define i32 @wyn_main() {
entry:
  %idx3 = alloca i32, align 4
  %idx2 = alloca i32, align 4
  %idx1 = alloca i32, align 4
  %sorted = alloca ptr, align 8
  %array_literal = alloca [8 x i32], align 4
  %element_ptr = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 1
  store i32 3, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 2
  store i32 5, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 3
  store i32 7, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 4
  store i32 9, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 5
  store i32 11, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 6
  store i32 13, ptr %element_ptr6, align 4
  %element_ptr7 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 7
  store i32 15, ptr %element_ptr7, align 4
  store ptr %array_literal, ptr %sorted, align 8
  %sorted8 = load ptr, ptr %sorted, align 8
  %binary_search = call i32 @binary_search(ptr %sorted8, i32 8, i32 7)
  store i32 %binary_search, ptr %idx1, align 4
  %sorted9 = load ptr, ptr %sorted, align 8
  %binary_search10 = call i32 @binary_search(ptr %sorted9, i32 8, i32 13)
  store i32 %binary_search10, ptr %idx2, align 4
  %sorted11 = load ptr, ptr %sorted, align 8
  %binary_search12 = call i32 @binary_search(ptr %sorted11, i32 8, i32 99)
  store i32 %binary_search12, ptr %idx3, align 4
  %idx113 = load i32, ptr %idx1, align 4
  %idx214 = load i32, ptr %idx2, align 4
  %add = add i32 %idx113, %idx214
  %idx315 = load i32, ptr %idx3, align 4
  %add16 = add i32 %add, %idx315
  ret i32 %add16
}

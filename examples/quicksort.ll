; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @partition(i32 %arr, i32 %low, i32 %high) {
entry:
  %temp28 = alloca i32, align 4
  %temp = alloca i32, align 4
  %j = alloca i32, align 4
  %i = alloca i32, align 4
  %pivot = alloca i32, align 4
  %high3 = alloca i32, align 4
  %low2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %low, ptr %low2, align 4
  store i32 %high, ptr %high3, align 4
  %arr4 = load i32, ptr %arr1, align 4
  %high5 = load i32, ptr %high3, align 4
  %array_element_ptr = getelementptr [0 x i32], i32 %arr4, 0, %high5
  %array_element = load i32, i32 %array_element_ptr, align 4
  store i32 %array_element, ptr %pivot, align 4
  %low6 = load i32, ptr %low2, align 4
  %sub = sub i32 %low6, 1
  store i32 %sub, ptr %i, align 4
  %low7 = load i32, ptr %low2, align 4
  store i32 %low7, ptr %j, align 4
  br label %while.header

while.header:                                     ; preds = %if.end, %entry
  %j8 = load i32, ptr %j, align 4
  %high9 = load i32, ptr %high3, align 4
  %icmp = icmp slt i32 %j8, %high9
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %arr10 = load i32, ptr %arr1, align 4
  %j11 = load i32, ptr %j, align 4
  %array_element_ptr12 = getelementptr [0 x i32], i32 %arr10, 0, %j11
  %array_element13 = load i32, i32 %array_element_ptr12, align 4
  %pivot14 = load i32, ptr %pivot, align 4
  %icmp15 = icmp slt i32 %array_element13, %pivot14
  br i1 %icmp15, label %if.then, label %if.end

while.end:                                        ; preds = %while.header
  %arr23 = load i32, ptr %arr1, align 4
  %i24 = load i32, ptr %i, align 4
  %add25 = add i32 %i24, 1
  %array_element_ptr26 = getelementptr [0 x i32], i32 %arr23, 0, %add25
  %array_element27 = load i32, i32 %array_element_ptr26, align 4
  store i32 %array_element27, ptr %temp28, align 4
  %i29 = load i32, ptr %i, align 4
  %add30 = add i32 %i29, 1
  ret i32 %add30

if.then:                                          ; preds = %while.body
  %i16 = load i32, ptr %i, align 4
  %add = add i32 %i16, 1
  store i32 %add, ptr %i, align 4
  %arr17 = load i32, ptr %arr1, align 4
  %i18 = load i32, ptr %i, align 4
  %array_element_ptr19 = getelementptr [0 x i32], i32 %arr17, 0, %i18
  %array_element20 = load i32, i32 %array_element_ptr19, align 4
  store i32 %array_element20, ptr %temp, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %j21 = load i32, ptr %j, align 4
  %add22 = add i32 %j21, 1
  store i32 %add22, ptr %j, align 4
  br label %while.header
}

define i32 @quicksort(i32 %arr, i32 %low, i32 %high) {
entry:
  %dummy2 = alloca i32, align 4
  %dummy1 = alloca i32, align 4
  %pi = alloca i32, align 4
  %high3 = alloca i32, align 4
  %low2 = alloca i32, align 4
  %arr1 = alloca i32, align 4
  store i32 %arr, ptr %arr1, align 4
  store i32 %low, ptr %low2, align 4
  store i32 %high, ptr %high3, align 4
  %low4 = load i32, ptr %low2, align 4
  %high5 = load i32, ptr %high3, align 4
  %icmp = icmp slt i32 %low4, %high5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %arr6 = load i32, ptr %arr1, align 4
  %low7 = load i32, ptr %low2, align 4
  %high8 = load i32, ptr %high3, align 4
  %partition = call i32 @partition(i32 %arr6, i32 %low7, i32 %high8)
  store i32 %partition, ptr %pi, align 4
  %arr9 = load i32, ptr %arr1, align 4
  %low10 = load i32, ptr %low2, align 4
  %pi11 = load i32, ptr %pi, align 4
  %sub = sub i32 %pi11, 1
  %quicksort = call i32 @quicksort(i32 %arr9, i32 %low10, i32 %sub)
  store i32 %quicksort, ptr %dummy1, align 4
  %arr12 = load i32, ptr %arr1, align 4
  %pi13 = load i32, ptr %pi, align 4
  %add = add i32 %pi13, 1
  %high14 = load i32, ptr %high3, align 4
  %quicksort15 = call i32 @quicksort(i32 %arr12, i32 %add, i32 %high14)
  store i32 %quicksort15, ptr %dummy2, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %dummy = alloca i32, align 4
  %n = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [7 x i32], align 4
  %element_ptr = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 0
  store i32 64, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 1
  store i32 34, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 2
  store i32 25, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 3
  store i32 12, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 4
  store i32 22, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 5
  store i32 11, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [7 x i32], ptr %array_literal, i32 0, i32 6
  store i32 90, ptr %element_ptr6, align 4
  store ptr %array_literal, ptr %arr, align 8
  store i32 7, ptr %n, align 4
  %arr7 = load ptr, ptr %arr, align 8
  %n8 = load i32, ptr %n, align 4
  %sub = sub i32 %n8, 1
  %quicksort = call i32 @quicksort(ptr %arr7, i32 0, i32 %sub)
  store i32 %quicksort, ptr %dummy, align 4
  %arr9 = load ptr, ptr %arr, align 8
  %array_element_ptr = getelementptr [0 x i32], ptr %arr9, i32 0, i32 0
  %array_element = load i32, ptr %array_element_ptr, align 4
  ret i32 %array_element
}

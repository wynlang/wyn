; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @abs_example(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %icmp = icmp slt i32 %x2, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %x3 = load i32, ptr %x1, align 4
  %sub = sub i32 0, %x3
  ret i32 %sub

if.end:                                           ; preds = %entry
  %x4 = load i32, ptr %x1, align 4
  ret i32 %x4
}

define i32 @max_example(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %icmp = icmp sgt i32 %a3, %b4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %a5 = load i32, ptr %a1, align 4
  ret i32 %a5

if.end:                                           ; preds = %entry
  %b6 = load i32, ptr %b2, align 4
  ret i32 %b6
}

define i32 @array_sum_example(i32 %arr, i32 %len) {
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

define i32 @array_max_example(i32 %arr, i32 %len) {
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

define i32 @is_leap_year(i32 %year) {
entry:
  %div4 = alloca i32, align 4
  %year1 = alloca i32, align 4
  store i32 %year, ptr %year1, align 4
  %year2 = load i32, ptr %year1, align 4
  %rem = srem i32 %year2, 4
  store i32 %rem, ptr %div4, align 4
  %div43 = load i32, ptr %div4, align 4
  %icmp = icmp eq i32 %div43, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @days_in_month(i32 %month) {
entry:
  %month1 = alloca i32, align 4
  store i32 %month, ptr %month1, align 4
  %month2 = load i32, ptr %month1, align 4
  %icmp = icmp eq i32 %month2, 2
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 28

if.end:                                           ; preds = %entry
  %month3 = load i32, ptr %month1, align 4
  %icmp4 = icmp eq i32 %month3, 4
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 30

if.end6:                                          ; preds = %if.end
  ret i32 31
}

define i32 @wyn_main() {
entry:
  %leap = alloca i32, align 4
  %days = alloca i32, align 4
  %diff = alloca i32, align 4
  %max_hours = alloca i32, align 4
  %total_hours = alloca i32, align 4
  %hours = alloca ptr, align 8
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 8, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 6, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 9, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 7, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 8, ptr %element_ptr4, align 4
  store ptr %array_literal, ptr %hours, align 8
  %hours5 = load ptr, ptr %hours, align 8
  %array_sum_example = call i32 @array_sum_example(ptr %hours5, i32 5)
  store i32 %array_sum_example, ptr %total_hours, align 4
  %hours6 = load ptr, ptr %hours, align 8
  %array_max_example = call i32 @array_max_example(ptr %hours6, i32 5)
  store i32 %array_max_example, ptr %max_hours, align 4
  %max_hours7 = load i32, ptr %max_hours, align 4
  %sub = sub i32 %max_hours7, 8
  %abs_example = call i32 @abs_example(i32 %sub)
  store i32 %abs_example, ptr %diff, align 4
  %days_in_month = call i32 @days_in_month(i32 1)
  store i32 %days_in_month, ptr %days, align 4
  %is_leap_year = call i32 @is_leap_year(i32 2024)
  store i32 %is_leap_year, ptr %leap, align 4
  %total_hours8 = load i32, ptr %total_hours, align 4
  %max_hours9 = load i32, ptr %max_hours, align 4
  %add = add i32 %total_hours8, %max_hours9
  %diff10 = load i32, ptr %diff, align 4
  %add11 = add i32 %add, %diff10
  %days12 = load i32, ptr %days, align 4
  %add13 = add i32 %add11, %days12
  %leap14 = load i32, ptr %leap, align 4
  %add15 = add i32 %add13, %leap14
  ret i32 %add15
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @abs_stdlib(i32 %x) {
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

define i32 @max_stdlib(i32 %a, i32 %b) {
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

define i32 @wyn_pow(i32 %x, i32 %n) {
entry:
  %i = alloca i32, align 4
  %result = alloca i32, align 4
  %n2 = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  store i32 %n, ptr %n2, align 4
  store i32 1, ptr %result, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %n4 = load i32, ptr %n2, align 4
  %icmp = icmp slt i32 %i3, %n4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %result5 = load i32, ptr %result, align 4
  %x6 = load i32, ptr %x1, align 4
  %mul = mul i32 %result5, %x6
  store i32 %mul, ptr %result, align 4
  %i7 = load i32, ptr %i, align 4
  %add = add i32 %i7, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %result8 = load i32, ptr %result, align 4
  ret i32 %result8
}

define i32 @array_sum_stdlib(i32 %arr, i32 %len) {
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

define i32 @wyn_main() {
entry:
  %s = alloca i32, align 4
  %arr = alloca ptr, align 8
  %p = alloca i32, align 4
  %m = alloca i32, align 4
  %a = alloca i32, align 4
  %abs_stdlib = call i32 @abs_stdlib(i32 -5)
  store i32 %abs_stdlib, ptr %a, align 4
  %max_stdlib = call i32 @max_stdlib(i32 10, i32 20)
  store i32 %max_stdlib, ptr %m, align 4
  %wyn_pow = call i32 @wyn_pow(i32 2, i32 3)
  store i32 %wyn_pow, ptr %p, align 4
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr4, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %array_sum_stdlib = call i32 @array_sum_stdlib(ptr %arr5, i32 5)
  store i32 %array_sum_stdlib, ptr %s, align 4
  %a6 = load i32, ptr %a, align 4
  %m7 = load i32, ptr %m, align 4
  %add = add i32 %a6, %m7
  %p8 = load i32, ptr %p, align 4
  %add9 = add i32 %add, %p8
  %s10 = load i32, ptr %s, align 4
  %add11 = add i32 %add9, %s10
  ret i32 %add11
}

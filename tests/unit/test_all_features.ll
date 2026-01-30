; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @add(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %add = add i32 %a3, %b4
  ret i32 %add
}

define i32 @generic_identity(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  ret i32 %x2
}

define i32 @wyn_main() {
entry:
  %gen_result = alloca i32, align 4
  %i = alloca i32, align 4
  %counter = alloca i32, align 4
  %result = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %status = alloca i32, align 4
  %px = alloca i32, align 4
  %p = alloca i32, align 4
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store i32 30, ptr %y, align 4
  %add = call i32 @add(i32 5, i32 7)
  store i32 %add, ptr %sum, align 4
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
  %array_element_ptr = getelementptr [0 x i32], ptr %arr5, i32 0, i32 0
  %array_element = load i32, ptr %array_element_ptr, align 4
  store i32 %array_element, ptr %first, align 4
  store i32 0, ptr %counter, align 4
  %result6 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result6, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %counter7 = load i32, ptr %counter, align 4
  %add8 = add i32 %counter7, 1
  store i32 %add8, ptr %counter, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %i9 = load i32, ptr %i, align 4
  %icmp10 = icmp slt i32 %i9, 3
  br i1 %icmp10, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter11 = load i32, ptr %counter, align 4
  %add12 = add i32 %counter11, 1
  store i32 %add12, ptr %counter, align 4
  %i13 = load i32, ptr %i, align 4
  %add14 = add i32 %i13, 1
  store i32 %add14, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %generic_identity = call i32 @generic_identity(i32 100)
  store i32 %generic_identity, ptr %gen_result, align 4
  %x15 = load i32, ptr %x, align 4
  %y16 = load i32, ptr %y, align 4
  %add17 = add i32 %x15, %y16
  %sum18 = load i32, ptr %sum, align 4
  %add19 = add i32 %add17, %sum18
  %px20 = load i32, ptr %px, align 4
  %add21 = add i32 %add19, %px20
  %result22 = load i32, ptr %result, align 4
  %add23 = add i32 %add21, %result22
  %counter24 = load i32, ptr %counter, align 4
  %add25 = add i32 %add23, %counter24
  %gen_result26 = load i32, ptr %gen_result, align 4
  %add27 = add i32 %add25, %gen_result26
  ret i32 %add27
}

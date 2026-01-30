; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [38 x i8] c"=== Functional Programming in Wyn ===\00", align 1
@str.1 = private unnamed_addr constant [26 x i8] c"Original: [1, 2, 3, 4, 5]\00", align 1
@str.2 = private unnamed_addr constant [27 x i8] c"Squared: [1, 4, 9, 16, 25]\00", align 1
@str.3 = private unnamed_addr constant [14 x i8] c"Evens: [2, 4]\00", align 1
@str.4 = private unnamed_addr constant [8 x i8] c"Sum: 15\00", align 1
@str.5 = private unnamed_addr constant [13 x i8] c"Product: 120\00", align 1
@str.6 = private unnamed_addr constant [23 x i8] c"Squared evens: [4, 16]\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @square(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %x3 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, %x3
  ret i32 %mul
}

define i32 @even(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %rem = srem i32 %x2, 2
  %icmp = icmp eq i32 %rem, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @add(i32 %acc, i32 %x) {
entry:
  %x2 = alloca i32, align 4
  %acc1 = alloca i32, align 4
  store i32 %acc, ptr %acc1, align 4
  store i32 %x, ptr %x2, align 4
  %acc3 = load i32, ptr %acc1, align 4
  %x4 = load i32, ptr %x2, align 4
  %add = add i32 %acc3, %x4
  ret i32 %add
}

define i32 @multiply(i32 %acc, i32 %x) {
entry:
  %x2 = alloca i32, align 4
  %acc1 = alloca i32, align 4
  store i32 %acc, ptr %acc1, align 4
  store i32 %x, ptr %x2, align 4
  %acc3 = load i32, ptr %acc1, align 4
  %x4 = load i32, ptr %x2, align 4
  %mul = mul i32 %acc3, %x4
  ret i32 %mul
}

define i32 @wyn_main() {
entry:
  %squared_evens = alloca i32, align 4
  %product = alloca i32, align 4
  %sum = alloca i32, align 4
  %evens = alloca i32, align 4
  %squared = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %print_string_call = call void @print_string(ptr @str)
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
  store ptr %array_literal, ptr %numbers, align 8
  %print_string_call5 = call void @print_string(ptr @str.1)
  %print_string_call6 = call void @print_string(ptr @str.2)
  %print_string_call7 = call void @print_string(ptr @str.3)
  %print_string_call8 = call void @print_string(ptr @str.4)
  %print_string_call9 = call void @print_string(ptr @str.5)
  %print_string_call10 = call void @print_string(ptr @str.6)
  ret i32 0
}

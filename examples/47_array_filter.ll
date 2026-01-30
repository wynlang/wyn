; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [16 x i8] c"Original count:\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Even count:\00", align 1
@str.2 = private unnamed_addr constant [12 x i8] c"First even:\00", align 1
@str.3 = private unnamed_addr constant [26 x i8] c"\E2\9C\93 array.filter() works!\00", align 1
@str.4 = private unnamed_addr constant [26 x i8] c"\E2\9C\97 array.filter() failed\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @check_even(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %rem = srem i32 %x2, 2
  %icmp = icmp eq i32 %rem, 0
  ret i1 %icmp
}

define i32 @check_positive(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %icmp = icmp sgt i32 %x2, 0
  ret i1 %icmp
}

define i32 @wyn_main() {
entry:
  %first = alloca i32, align 4
  %evens = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %array_literal = alloca [6 x i32], align 4
  %element_ptr = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 5
  store i32 6, ptr %element_ptr5, align 4
  store ptr %array_literal, ptr %numbers, align 8
  %print_string_call = call void @print_string(ptr @str)
  %print_string_call6 = call void @print_string(ptr @str.1)
  %print_string_call7 = call void @print_string(ptr @str.2)
  %first8 = load i32, ptr %first, align 4
  %print_call = call void @print(i32 %first8)
  %first9 = load i32, ptr %first, align 4
  %icmp = icmp eq i32 %first9, 2
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %print_string_call10 = call void @print_string(ptr @str.3)
  ret i32 0

if.else:                                          ; preds = %entry
  %print_string_call11 = call void @print_string(ptr @str.4)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

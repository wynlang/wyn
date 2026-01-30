; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [5 x i8] c"Sum:\00", align 1
@str.1 = private unnamed_addr constant [9 x i8] c"Product:\00", align 1
@str.2 = private unnamed_addr constant [26 x i8] c"\E2\9C\93 array.reduce() works!\00", align 1
@str.3 = private unnamed_addr constant [26 x i8] c"\E2\9C\97 array.reduce() failed\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

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
  %product = alloca i32, align 4
  %sum = alloca i32, align 4
  %numbers = alloca ptr, align 8
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
  %print_string_call = call void @print_string(ptr @str)
  %sum5 = load i32, ptr %sum, align 4
  %print_call = call void @print(i32 %sum5)
  %print_string_call6 = call void @print_string(ptr @str.1)
  %product7 = load i32, ptr %product, align 4
  %print_call8 = call void @print(i32 %product7)
  %sum9 = load i32, ptr %sum, align 4
  %icmp = icmp eq i32 %sum9, 15
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %print_string_call10 = call void @print_string(ptr @str.2)
  ret i32 0

if.else:                                          ; preds = %entry
  %print_string_call11 = call void @print_string(ptr @str.3)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

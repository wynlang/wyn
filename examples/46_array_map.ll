; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [10 x i8] c"Original:\00", align 1
@str.1 = private unnamed_addr constant [9 x i8] c"Doubled:\00", align 1
@str.2 = private unnamed_addr constant [15 x i8] c"First doubled:\00", align 1
@str.3 = private unnamed_addr constant [23 x i8] c"\E2\9C\93 array.map() works!\00", align 1
@str.4 = private unnamed_addr constant [23 x i8] c"\E2\9C\97 array.map() failed\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @double_it(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, 2
  ret i32 %mul
}

define i32 @square(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %x3 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, %x3
  ret i32 %mul
}

define i32 @wyn_main() {
entry:
  %first = alloca i32, align 4
  %doubled = alloca i32, align 4
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
  %print_string_call5 = call void @print_string(ptr @str.1)
  %print_string_call6 = call void @print_string(ptr @str.2)
  %first7 = load i32, ptr %first, align 4
  %print_call = call void @print(i32 %first7)
  %first8 = load i32, ptr %first, align 4
  %icmp = icmp eq i32 %first8, 2
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %print_string_call9 = call void @print_string(ptr @str.3)
  ret i32 0

if.else:                                          ; preds = %entry
  %print_string_call10 = call void @print_string(ptr @str.4)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

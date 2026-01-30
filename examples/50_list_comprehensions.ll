; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @twice(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, 2
  ret i32 %mul
}

define i32 @even_only(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %rem = srem i32 %x2, 2
  %icmp = icmp eq i32 %rem, 0
  ret i1 %icmp
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

define void @wyn_main() {
entry:
  %result = alloca i32, align 4
  %sum = alloca i32, align 4
  %evens = alloca i32, align 4
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
  %doubled5 = load i32, ptr %doubled, align 4
  %print_call = call void @print(i32 %doubled5)
  %evens6 = load i32, ptr %evens, align 4
  %print_call7 = call void @print(i32 %evens6)
  %sum8 = load i32, ptr %sum, align 4
  %print_call9 = call void @print(i32 %sum8)
  %result10 = load i32, ptr %result, align 4
  %print_call11 = call void @print(i32 %result10)
  ret i32 0
}

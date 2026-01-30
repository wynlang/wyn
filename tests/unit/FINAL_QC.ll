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

define i32 @identity(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  ret i32 %x2
}

define i32 @wyn_main() {
entry:
  %opt = alloca ptr, align 8
  %matched = alloca i32, align 4
  %arr = alloca ptr, align 8
  %val = alloca i32, align 4
  %sum = alloca i32, align 4
  %p = alloca i32, align 4
  %sum1 = load i32, ptr %sum, align 4
  %identity = call i32 @identity(i32 %sum1)
  store i32 %identity, ptr %val, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 10, ptr %element_ptr, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 20, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 30, ptr %element_ptr3, align 4
  store ptr %array_literal, ptr %arr, align 8
  %matched4 = load i32, ptr %matched, align 4
  %tmp = alloca i32, align 4
  store i32 %matched4, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %opt, align 8
  ret i32 0
}

declare ptr @wyn_some(ptr, i64)

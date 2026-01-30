; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @zero() {
entry:
  ret i32 0
}

define i32 @nested_call() {
entry:
  %zero = call i32 @zero()
  ret i32 %zero
}

define i32 @generic_chain(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  ret i32 %x2
}

define i32 @wyn_main() {
entry:
  %single = alloca i32, align 4
  %arr = alloca ptr, align 8
  %i = alloca i32, align 4
  %z = alloca i32, align 4
  %p = alloca i32, align 4
  %e = alloca i32, align 4
  %nested_call = call i32 @nested_call()
  store i32 %nested_call, ptr %z, align 4
  %generic_chain = call i32 @generic_chain(i32 5)
  store i32 %generic_chain, ptr %i, align 4
  %array_literal = alloca [1 x i32], align 4
  %element_ptr = getelementptr [1 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  store ptr %array_literal, ptr %arr, align 8
  ret i32 0
}

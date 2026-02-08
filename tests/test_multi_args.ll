; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare noalias ptr @malloc(i64)

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

define i32 @multiply(i32 %a, i32 %b, i32 %c) {
entry:
  %c3 = alloca i32, align 4
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  store i32 %c, ptr %c3, align 4
  %a4 = load i32, ptr %a1, align 4
  %b5 = load i32, ptr %b2, align 4
  %mul = mul i32 %a4, %b5
  %c6 = load i32, ptr %c3, align 4
  %mul7 = mul i32 %mul, %c6
  ret i32 %mul7
}

define i32 @wyn_main() {
entry:
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %f2 = alloca ptr, align 8
  %f1 = alloca ptr, align 8
  %future = call ptr @wyn_spawn_async(ptr @__spawn_add_0, ptr null)
  store ptr %future, ptr %f1, align 8
  %future1 = call ptr @wyn_spawn_async(ptr @__spawn_multiply_1, ptr null)
  store ptr %future1, ptr %f2, align 8
  %f12 = load ptr, ptr %f1, align 8
  %result_ptr = call ptr @future_get(ptr %f12)
  %result = load i32, ptr %result_ptr, align 4
  call void @free(ptr %result_ptr)
  store i32 %result, ptr %r1, align 4
  %f23 = load ptr, ptr %f2, align 8
  %result_ptr4 = call ptr @future_get(ptr %f23)
  %result5 = load i32, ptr %result_ptr4, align 4
  call void @free(ptr %result_ptr4)
  store i32 %result5, ptr %r2, align 4
  %r16 = load i32, ptr %r1, align 4
  %icmp = icmp eq i32 %r16, 30
  %r27 = load i32, ptr %r2, align 4
  %icmp8 = icmp eq i32 %r27, 24
  %and = and i1 %icmp, %icmp8
  br i1 %and, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_add_0(ptr %0) {
entry:
  %1 = call i32 @add(i32 10, i32 20)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_multiply_1(ptr %0) {
entry:
  %1 = call i32 @multiply(i32 2, i32 3, i32 4)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

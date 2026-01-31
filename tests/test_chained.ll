; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare noalias ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @compute(i32 %n) {
entry:
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n2 = load i32, ptr %n1, align 4
  %n3 = load i32, ptr %n1, align 4
  %mul = mul i32 %n2, %n3
  ret i32 %mul
}

define i32 @wyn_main() {
entry:
  %r2 = alloca i32, align 4
  %f2 = alloca ptr, align 8
  %r1 = alloca i32, align 4
  %f1 = alloca ptr, align 8
  %future = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  store ptr %future, ptr %f1, align 8
  %f11 = load ptr, ptr %f1, align 8
  %result_ptr = call ptr @future_get(ptr %f11)
  %result = load i32, ptr %result_ptr, align 4
  call void @free(ptr %result_ptr)
  store i32 %result, ptr %r1, align 4
  %future2 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  store ptr %future2, ptr %f2, align 8
  %f23 = load ptr, ptr %f2, align 8
  %result_ptr4 = call ptr @future_get(ptr %f23)
  %result5 = load i32, ptr %result_ptr4, align 4
  call void @free(ptr %result_ptr4)
  store i32 %result5, ptr %r2, align 4
  %r26 = load i32, ptr %r2, align 4
  %icmp = icmp eq i32 %r26, 625
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %1 = call i32 @compute(i32 5)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

define ptr @__spawn_compute_1(ptr %0) {
entry:
  %r1 = load i32, ptr %r1, align 4
  %1 = call i32 @compute(i32 %r1)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

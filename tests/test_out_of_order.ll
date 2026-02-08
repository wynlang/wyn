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
  %r1 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r3 = alloca i32, align 4
  %f3 = alloca ptr, align 8
  %f2 = alloca ptr, align 8
  %f1 = alloca ptr, align 8
  %future = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  store ptr %future, ptr %f1, align 8
  %future1 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  store ptr %future1, ptr %f2, align 8
  %future2 = call ptr @wyn_spawn_async(ptr @__spawn_compute_2, ptr null)
  store ptr %future2, ptr %f3, align 8
  %f33 = load ptr, ptr %f3, align 8
  %result_ptr = call ptr @future_get(ptr %f33)
  %result = load i32, ptr %result_ptr, align 4
  call void @free(ptr %result_ptr)
  store i32 %result, ptr %r3, align 4
  %f24 = load ptr, ptr %f2, align 8
  %result_ptr5 = call ptr @future_get(ptr %f24)
  %result6 = load i32, ptr %result_ptr5, align 4
  call void @free(ptr %result_ptr5)
  store i32 %result6, ptr %r2, align 4
  %f17 = load ptr, ptr %f1, align 8
  %result_ptr8 = call ptr @future_get(ptr %f17)
  %result9 = load i32, ptr %result_ptr8, align 4
  call void @free(ptr %result_ptr8)
  store i32 %result9, ptr %r1, align 4
  %r110 = load i32, ptr %r1, align 4
  %icmp = icmp eq i32 %r110, 1
  %r211 = load i32, ptr %r2, align 4
  %icmp12 = icmp eq i32 %r211, 4
  %and = and i1 %icmp, %icmp12
  %r313 = load i32, ptr %r3, align 4
  %icmp14 = icmp eq i32 %r313, 9
  %and15 = and i1 %and, %icmp14
  br i1 %and15, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %1 = call i32 @compute(i32 1)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_1(ptr %0) {
entry:
  %1 = call i32 @compute(i32 2)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_2(ptr %0) {
entry:
  %1 = call i32 @compute(i32 3)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

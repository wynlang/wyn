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
  %r5 = alloca i32, align 4
  %r4 = alloca i32, align 4
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %f5 = alloca ptr, align 8
  %f4 = alloca ptr, align 8
  %f3 = alloca ptr, align 8
  %f2 = alloca ptr, align 8
  %f1 = alloca ptr, align 8
  %future = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  store ptr %future, ptr %f1, align 8
  %future1 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  store ptr %future1, ptr %f2, align 8
  %future2 = call ptr @wyn_spawn_async(ptr @__spawn_compute_2, ptr null)
  store ptr %future2, ptr %f3, align 8
  %future3 = call ptr @wyn_spawn_async(ptr @__spawn_compute_3, ptr null)
  store ptr %future3, ptr %f4, align 8
  %future4 = call ptr @wyn_spawn_async(ptr @__spawn_compute_4, ptr null)
  store ptr %future4, ptr %f5, align 8
  %f15 = load ptr, ptr %f1, align 8
  %result_ptr = call ptr @future_get(ptr %f15)
  %result = load i32, ptr %result_ptr, align 4
  call void @free(ptr %result_ptr)
  store i32 %result, ptr %r1, align 4
  %f26 = load ptr, ptr %f2, align 8
  %result_ptr7 = call ptr @future_get(ptr %f26)
  %result8 = load i32, ptr %result_ptr7, align 4
  call void @free(ptr %result_ptr7)
  store i32 %result8, ptr %r2, align 4
  %f39 = load ptr, ptr %f3, align 8
  %result_ptr10 = call ptr @future_get(ptr %f39)
  %result11 = load i32, ptr %result_ptr10, align 4
  call void @free(ptr %result_ptr10)
  store i32 %result11, ptr %r3, align 4
  %f412 = load ptr, ptr %f4, align 8
  %result_ptr13 = call ptr @future_get(ptr %f412)
  %result14 = load i32, ptr %result_ptr13, align 4
  call void @free(ptr %result_ptr13)
  store i32 %result14, ptr %r4, align 4
  %f515 = load ptr, ptr %f5, align 8
  %result_ptr16 = call ptr @future_get(ptr %f515)
  %result17 = load i32, ptr %result_ptr16, align 4
  call void @free(ptr %result_ptr16)
  store i32 %result17, ptr %r5, align 4
  %r118 = load i32, ptr %r1, align 4
  %icmp = icmp eq i32 %r118, 1
  %r219 = load i32, ptr %r2, align 4
  %icmp20 = icmp eq i32 %r219, 4
  %and = and i1 %icmp, %icmp20
  %r321 = load i32, ptr %r3, align 4
  %icmp22 = icmp eq i32 %r321, 9
  %and23 = and i1 %and, %icmp22
  %r424 = load i32, ptr %r4, align 4
  %icmp25 = icmp eq i32 %r424, 16
  %and26 = and i1 %and23, %icmp25
  %r527 = load i32, ptr %r5, align 4
  %icmp28 = icmp eq i32 %r527, 25
  %and29 = and i1 %and26, %icmp28
  br i1 %and29, label %if.then, label %if.end

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

define ptr @__spawn_compute_3(ptr %0) {
entry:
  %1 = call i32 @compute(i32 4)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_4(ptr %0) {
entry:
  %1 = call i32 @compute(i32 5)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

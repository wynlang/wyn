; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

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

define i32 @wyn_main() {
entry:
  %r6 = alloca i32, align 4
  %f6 = alloca ptr, align 8
  %r5 = alloca i32, align 4
  %f5 = alloca ptr, align 8
  %r4 = alloca i32, align 4
  %f4 = alloca ptr, align 8
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %f3 = alloca ptr, align 8
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
  %r12 = load i32, ptr %r1, align 4
  %icmp = icmp ne i32 %r12, 100
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  %future3 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  store ptr %future3, ptr %f2, align 8
  %future4 = call ptr @wyn_spawn_async(ptr @__spawn_compute_2, ptr null)
  store ptr %future4, ptr %f3, align 8
  %f25 = load ptr, ptr %f2, align 8
  %result_ptr6 = call ptr @future_get(ptr %f25)
  %result7 = load i32, ptr %result_ptr6, align 4
  call void @free(ptr %result_ptr6)
  store i32 %result7, ptr %r2, align 4
  %f38 = load ptr, ptr %f3, align 8
  %result_ptr9 = call ptr @future_get(ptr %f38)
  %result10 = load i32, ptr %result_ptr9, align 4
  call void @free(ptr %result_ptr9)
  store i32 %result10, ptr %r3, align 4
  %r211 = load i32, ptr %r2, align 4
  %icmp12 = icmp ne i32 %r211, 400
  br i1 %icmp12, label %if.then13, label %if.end14

if.then13:                                        ; preds = %if.end
  ret i32 2

if.end14:                                         ; preds = %if.end
  %r315 = load i32, ptr %r3, align 4
  %icmp16 = icmp ne i32 %r315, 900
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end14
  ret i32 3

if.end18:                                         ; preds = %if.end14
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i32 2)
  %future19 = call ptr @wyn_spawn_async(ptr @__spawn_add_3, ptr null)
  store ptr %future19, ptr %f4, align 8
  %f420 = load ptr, ptr %f4, align 8
  %result_ptr21 = call ptr @future_get(ptr %f420)
  %result22 = load i32, ptr %result_ptr21, align 4
  call void @free(ptr %result_ptr21)
  store i32 %result22, ptr %r4, align 4
  %r423 = load i32, ptr %r4, align 4
  %icmp24 = icmp ne i32 %r423, 30
  br i1 %icmp24, label %if.then25, label %if.end26

if.then25:                                        ; preds = %if.end18
  ret i32 4

if.end26:                                         ; preds = %if.end18
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 3)
  %future27 = call ptr @wyn_spawn_async(ptr @__spawn_compute_4, ptr null)
  store ptr %future27, ptr %f5, align 8
  %f528 = load ptr, ptr %f5, align 8
  %result_ptr29 = call ptr @future_get(ptr %f528)
  %result30 = load i32, ptr %result_ptr29, align 4
  call void @free(ptr %result_ptr29)
  store i32 %result30, ptr %r5, align 4
  %future31 = call ptr @wyn_spawn_async(ptr @__spawn_compute_5, ptr null)
  store ptr %future31, ptr %f6, align 8
  %f632 = load ptr, ptr %f6, align 8
  %result_ptr33 = call ptr @future_get(ptr %f632)
  %result34 = load i32, ptr %result_ptr33, align 4
  call void @free(ptr %result_ptr33)
  store i32 %result34, ptr %r6, align 4
  %r635 = load i32, ptr %r6, align 4
  %icmp36 = icmp ne i32 %r635, 625
  br i1 %icmp36, label %if.then37, label %if.end38

if.then37:                                        ; preds = %if.end26
  ret i32 5

if.end38:                                         ; preds = %if.end26
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 4)
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %1 = call i32 @compute(i32 10)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

define ptr @__spawn_compute_1(ptr %0) {
entry:
  %1 = call i32 @compute(i32 20)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_2(ptr %0) {
entry:
  %1 = call i32 @compute(i32 30)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_add_3(ptr %0) {
entry:
  %1 = call i32 @add(i32 10, i32 20)
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

define ptr @__spawn_compute_5(ptr %0) {
entry:
  %r5 = load i32, ptr %r5, align 4
  %1 = call i32 @compute(i32 %r5)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

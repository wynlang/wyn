; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

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
  %ra = alloca i32, align 4
  %rb = alloca i32, align 4
  %rc = alloca i32, align 4
  %fc = alloca ptr, align 8
  %fb = alloca ptr, align 8
  %fa = alloca ptr, align 8
  %r7 = alloca i32, align 4
  %f7 = alloca ptr, align 8
  %r6 = alloca i32, align 4
  %f6 = alloca ptr, align 8
  %r5 = alloca i32, align 4
  %f5 = alloca ptr, align 8
  %r4 = alloca i32, align 4
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %f4 = alloca ptr, align 8
  %f3 = alloca ptr, align 8
  %f2 = alloca ptr, align 8
  %result1 = alloca i32, align 4
  %f1 = alloca ptr, align 8
  %future = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  store ptr %future, ptr %f1, align 8
  %f11 = load ptr, ptr %f1, align 8
  %result_ptr = call ptr @future_get(ptr %f11)
  %result = load i32, ptr %result_ptr, align 4
  call void @free(ptr %result_ptr)
  store i32 %result, ptr %result1, align 4
  %result12 = load i32, ptr %result1, align 4
  %icmp = icmp ne i32 %result12, 1764
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  %future3 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  store ptr %future3, ptr %f2, align 8
  %future4 = call ptr @wyn_spawn_async(ptr @__spawn_compute_2, ptr null)
  store ptr %future4, ptr %f3, align 8
  %future5 = call ptr @wyn_spawn_async(ptr @__spawn_compute_3, ptr null)
  store ptr %future5, ptr %f4, align 8
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
  %r215 = load i32, ptr %r2, align 4
  %icmp16 = icmp ne i32 %r215, 100
  %r317 = load i32, ptr %r3, align 4
  %icmp18 = icmp ne i32 %r317, 400
  %or = or i1 %icmp16, %icmp18
  %r419 = load i32, ptr %r4, align 4
  %icmp20 = icmp ne i32 %r419, 900
  %or21 = or i1 %or, %icmp20
  br i1 %or21, label %if.then22, label %if.end23

if.then22:                                        ; preds = %if.end
  ret i32 2

if.end23:                                         ; preds = %if.end
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i32 2)
  %future24 = call ptr @wyn_spawn_async(ptr @__spawn_add_4, ptr null)
  store ptr %future24, ptr %f5, align 8
  %f525 = load ptr, ptr %f5, align 8
  %result_ptr26 = call ptr @future_get(ptr %f525)
  %result27 = load i32, ptr %result_ptr26, align 4
  call void @free(ptr %result_ptr26)
  store i32 %result27, ptr %r5, align 4
  %r528 = load i32, ptr %r5, align 4
  %icmp29 = icmp ne i32 %r528, 300
  br i1 %icmp29, label %if.then30, label %if.end31

if.then30:                                        ; preds = %if.end23
  ret i32 3

if.end31:                                         ; preds = %if.end23
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 3)
  %future32 = call ptr @wyn_spawn_async(ptr @__spawn_compute_5, ptr null)
  store ptr %future32, ptr %f6, align 8
  %f633 = load ptr, ptr %f6, align 8
  %result_ptr34 = call ptr @future_get(ptr %f633)
  %result35 = load i32, ptr %result_ptr34, align 4
  call void @free(ptr %result_ptr34)
  store i32 %result35, ptr %r6, align 4
  %future36 = call ptr @wyn_spawn_async(ptr @__spawn_compute_6, ptr null)
  store ptr %future36, ptr %f7, align 8
  %f737 = load ptr, ptr %f7, align 8
  %result_ptr38 = call ptr @future_get(ptr %f737)
  %result39 = load i32, ptr %result_ptr38, align 4
  call void @free(ptr %result_ptr38)
  store i32 %result39, ptr %r7, align 4
  %r740 = load i32, ptr %r7, align 4
  %icmp41 = icmp ne i32 %r740, 625
  br i1 %icmp41, label %if.then42, label %if.end43

if.then42:                                        ; preds = %if.end31
  ret i32 4

if.end43:                                         ; preds = %if.end31
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 4)
  %future44 = call ptr @wyn_spawn_async(ptr @__spawn_compute_7, ptr null)
  store ptr %future44, ptr %fa, align 8
  %future45 = call ptr @wyn_spawn_async(ptr @__spawn_compute_8, ptr null)
  store ptr %future45, ptr %fb, align 8
  %future46 = call ptr @wyn_spawn_async(ptr @__spawn_compute_9, ptr null)
  store ptr %future46, ptr %fc, align 8
  %fc47 = load ptr, ptr %fc, align 8
  %result_ptr48 = call ptr @future_get(ptr %fc47)
  %result49 = load i32, ptr %result_ptr48, align 4
  call void @free(ptr %result_ptr48)
  store i32 %result49, ptr %rc, align 4
  %fb50 = load ptr, ptr %fb, align 8
  %result_ptr51 = call ptr @future_get(ptr %fb50)
  %result52 = load i32, ptr %result_ptr51, align 4
  call void @free(ptr %result_ptr51)
  store i32 %result52, ptr %rb, align 4
  %fa53 = load ptr, ptr %fa, align 8
  %result_ptr54 = call ptr @future_get(ptr %fa53)
  %result55 = load i32, ptr %result_ptr54, align 4
  call void @free(ptr %result_ptr54)
  store i32 %result55, ptr %ra, align 4
  %ra56 = load i32, ptr %ra, align 4
  %icmp57 = icmp ne i32 %ra56, 1
  %rb58 = load i32, ptr %rb, align 4
  %icmp59 = icmp ne i32 %rb58, 4
  %or60 = or i1 %icmp57, %icmp59
  %rc61 = load i32, ptr %rc, align 4
  %icmp62 = icmp ne i32 %rc61, 9
  %or63 = or i1 %or60, %icmp62
  br i1 %or63, label %if.then64, label %if.end65

if.then64:                                        ; preds = %if.end43
  ret i32 5

if.end65:                                         ; preds = %if.end43
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.4, i32 5)
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 0)
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %1 = call i32 @compute(i32 42)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

declare ptr @future_get(ptr)

define ptr @__spawn_compute_1(ptr %0) {
entry:
  %1 = call i32 @compute(i32 10)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_2(ptr %0) {
entry:
  %1 = call i32 @compute(i32 20)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_3(ptr %0) {
entry:
  %1 = call i32 @compute(i32 30)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_add_4(ptr %0) {
entry:
  %1 = call i32 @add(i32 100, i32 200)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_5(ptr %0) {
entry:
  %1 = call i32 @compute(i32 5)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_6(ptr %0) {
entry:
  %r6 = load i32, ptr %r6, align 4
  %1 = call i32 @compute(i32 %r6)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_7(ptr %0) {
entry:
  %1 = call i32 @compute(i32 1)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_8(ptr %0) {
entry:
  %1 = call i32 @compute(i32 2)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_9(ptr %0) {
entry:
  %1 = call i32 @compute(i32 3)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

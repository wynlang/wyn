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

define i32 @noop() {
entry:
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %wait = alloca i32, align 4
  %i = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  %1 = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  %2 = call ptr @wyn_spawn_async(ptr @__spawn_compute_1, ptr null)
  %3 = call ptr @wyn_spawn_async(ptr @__spawn_add_2, ptr null)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.1, i32 2)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 10
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %5 = call ptr @wyn_spawn_async(ptr @__spawn_noop_3, ptr null)
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 3)
  store i32 0, ptr %i, align 4
  br label %while.header3

while.header3:                                    ; preds = %while.body4, %while.end
  %i6 = load i32, ptr %i, align 4
  %icmp7 = icmp slt i32 %i6, 5
  br i1 %icmp7, label %while.body4, label %while.end5

while.body4:                                      ; preds = %while.header3
  %7 = call ptr @wyn_spawn_async(ptr @__spawn_compute_4, ptr null)
  %i8 = load i32, ptr %i, align 4
  %add9 = add i32 %i8, 1
  store i32 %add9, ptr %i, align 4
  br label %while.header3

while.end5:                                       ; preds = %while.header3
  store i32 0, ptr %wait, align 4
  br label %while.header10

while.header10:                                   ; preds = %while.body11, %while.end5
  %wait13 = load i32, ptr %wait, align 4
  %icmp14 = icmp slt i32 %wait13, 10000000
  br i1 %icmp14, label %while.body11, label %while.end12

while.body11:                                     ; preds = %while.header10
  %wait15 = load i32, ptr %wait, align 4
  %add16 = add i32 %wait15, 1
  store i32 %add16, ptr %wait, align 4
  br label %while.header10

while.end12:                                      ; preds = %while.header10
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 4)
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

define ptr @__spawn_compute_1(ptr %0) {
entry:
  %1 = call i32 @compute(i32 20)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_add_2(ptr %0) {
entry:
  %1 = call i32 @add(i32 5, i32 7)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_noop_3(ptr %0) {
entry:
  %1 = call i32 @noop()
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_compute_4(ptr %0) {
entry:
  %i = load i32, ptr %i, align 4
  %1 = call i32 @compute(i32 %i)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

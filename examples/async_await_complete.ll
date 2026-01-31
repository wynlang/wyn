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
@fmt.6 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

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

define i32 @fetch_data(i32 %id) {
entry:
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  %mul = mul i32 %id2, 10
  ret i32 %mul
}

define i32 @regular_function() {
entry:
  ret i32 42
}

define i32 @wyn_main() {
entry:
  %i = alloca i32, align 4
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  %compute = call i32 @compute(i32 10)
  store i32 %compute, ptr %r1, align 4
  %r11 = load i32, ptr %r1, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i32 %r11)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 2)
  %3 = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 3)
  %fetch_data = call i32 @fetch_data(i32 5)
  store i32 %fetch_data, ptr %r2, align 4
  %r22 = load i32, ptr %r2, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.4, i32 %r22)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 4)
  %regular_function = call i32 @regular_function()
  store i32 %regular_function, ptr %r3, align 4
  %r33 = load i32, ptr %r3, align 4
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.6, i32 %r33)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i4 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i4, 1000000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i5 = load i32, ptr %i, align 4
  %add = add i32 %i5, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.7, i32 5)
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %1 = call i32 @compute(i32 20)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

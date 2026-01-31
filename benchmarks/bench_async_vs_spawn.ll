; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

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
  %spawn_time = alloca i64, align 8
  %seq_time = alloca i64, align 8
  %i = alloca i32, align 4
  %start = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 10000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i2 = load i32, ptr %i, align 4
  %compute = call i32 @compute(i32 %i2)
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %i3, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %time_now4 = call i64 @wyn_time_now()
  %start5 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now4, %start5
  store i64 %sub, ptr %seq_time, align 4
  %seq_time6 = load i64, ptr %seq_time, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i64 %seq_time6)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 2)
  %time_now7 = call i64 @wyn_time_now()
  store i64 %time_now7, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header8

while.header8:                                    ; preds = %while.body9, %while.end
  %i11 = load i32, ptr %i, align 4
  %icmp12 = icmp slt i32 %i11, 10000
  br i1 %icmp12, label %while.body9, label %while.end10

while.body9:                                      ; preds = %while.header8
  %3 = call ptr @wyn_spawn_async(ptr @__spawn_compute_0, ptr null)
  %i13 = load i32, ptr %i, align 4
  %add14 = add i32 %i13, 1
  store i32 %add14, ptr %i, align 4
  br label %while.header8

while.end10:                                      ; preds = %while.header8
  %time_now15 = call i64 @wyn_time_now()
  %start16 = load i64, ptr %start, align 4
  %sub17 = sub i64 %time_now15, %start16
  store i64 %sub17, ptr %spawn_time, align 4
  %spawn_time18 = load i64, ptr %spawn_time, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.3, i64 %spawn_time18)
  store i32 0, ptr %i, align 4
  br label %while.header19

while.header19:                                   ; preds = %while.body20, %while.end10
  %i22 = load i32, ptr %i, align 4
  %icmp23 = icmp slt i32 %i22, 10000000
  br i1 %icmp23, label %while.body20, label %while.end21

while.body20:                                     ; preds = %while.header19
  %i24 = load i32, ptr %i, align 4
  %add25 = add i32 %i24, 1
  store i32 %add25, ptr %i, align 4
  br label %while.header19

while.end21:                                      ; preds = %while.header19
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.4, i32 3)
  ret i32 0
}

declare i64 @wyn_time_now()

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_compute_0(ptr %0) {
entry:
  %i = load i32, ptr %i, align 4
  %1 = call i32 @compute(i32 %i)
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

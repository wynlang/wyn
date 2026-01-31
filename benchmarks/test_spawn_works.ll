; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @work() {
entry:
  %i = alloca i32, align 4
  %sum = alloca i32, align 4
  store i32 0, ptr %sum, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 1000000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %sum2 = load i32, ptr %sum, align 4
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %sum2, %i3
  store i32 %add, ptr %sum, align 4
  %i4 = load i32, ptr %i, align 4
  %add5 = add i32 %i4, 1
  store i32 %add5, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %sum6 = load i32, ptr %sum, align 4
  ret i32 %sum6
}

define i32 @wyn_main() {
entry:
  %elapsed = alloca i64, align 8
  %j = alloca i32, align 4
  %wait_sum = alloca i32, align 4
  %start = alloca i64, align 8
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  call void @wyn_spawn(ptr @__spawn_work_0, ptr null)
  call void @wyn_spawn(ptr @__spawn_work_1, ptr null)
  call void @wyn_spawn(ptr @__spawn_work_2, ptr null)
  call void @wyn_spawn(ptr @__spawn_work_3, ptr null)
  store i32 0, ptr %wait_sum, align 4
  store i32 0, ptr %j, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %j1 = load i32, ptr %j, align 4
  %icmp = icmp slt i32 %j1, 10000000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %wait_sum2 = load i32, ptr %wait_sum, align 4
  %add = add i32 %wait_sum2, 1
  store i32 %add, ptr %wait_sum, align 4
  %j3 = load i32, ptr %j, align 4
  %add4 = add i32 %j3, 1
  store i32 %add4, ptr %j, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %time_now5 = call i64 @wyn_time_now()
  %start6 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now5, %start6
  store i64 %sub, ptr %elapsed, align 4
  %elapsed7 = load i64, ptr %elapsed, align 4
  %div = sdiv i64 %elapsed7, i32 1000000
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i64 %div)
  ret i32 0
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

define ptr @__spawn_work_0(ptr %0) {
entry:
  %1 = call i32 @work()
  ret ptr null
}

define ptr @__spawn_work_1(ptr %0) {
entry:
  %1 = call i32 @work()
  ret ptr null
}

define ptr @__spawn_work_2(ptr %0) {
entry:
  %1 = call i32 @work()
  ret ptr null
}

define ptr @__spawn_work_3(ptr %0) {
entry:
  %1 = call i32 @work()
  ret ptr null
}

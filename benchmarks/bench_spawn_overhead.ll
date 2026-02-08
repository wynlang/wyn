; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare noalias ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @noop() {
entry:
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %elapsed = alloca i64, align 8
  %i = alloca i32, align 4
  %start = alloca i64, align 8
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 10000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %0 = call ptr @wyn_spawn_async(ptr @__spawn_noop_0, ptr null)
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %time_now3 = call i64 @wyn_time_now()
  %start4 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now3, %start4
  store i64 %sub, ptr %elapsed, align 4
  %elapsed5 = load i64, ptr %elapsed, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt, i64 %elapsed5)
  ret i32 0
}

declare i64 @wyn_time_now()

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_noop_0(ptr %0) {
entry:
  %1 = call i32 @noop()
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [16 x i8] c"10,000 spawns: \00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.2 = private unnamed_addr constant [5 x i8] c"ms\\n\00", align 1
@fmt.3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @worker(i32 %id) {
entry:
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  ret i32 %id2
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
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %time_now3 = call i64 @wyn_time_now()
  %start4 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now3, %start4
  store i64 %sub, ptr %elapsed, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %elapsed5 = load i64, ptr %elapsed, align 4
  %div = sdiv i64 %elapsed5, i32 1000000
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i64 %div)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.3, ptr @str.2)
  ret i32 0
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

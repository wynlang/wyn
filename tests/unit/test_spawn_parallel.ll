; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"Worker done\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [30 x i8] c"Testing parallel execution...\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [10 x i8] c"Duration:\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.9 = private unnamed_addr constant [27 x i8] c"\E2\9C\93 Tasks ran in parallel!\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [27 x i8] c"\E2\9C\97 Tasks ran sequentially\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @slow_worker() {
entry:
  %0 = call i32 @usleep(i32 100000)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %duration = alloca i64, align 8
  %end = alloca i64, align 8
  %start = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %1 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  %slow_worker = call i32 @slow_worker()
  %slow_worker1 = call i32 @slow_worker()
  %slow_worker2 = call i32 @slow_worker()
  %slow_worker3 = call i32 @slow_worker()
  %slow_worker4 = call i32 @slow_worker()
  %slow_worker5 = call i32 @slow_worker()
  %slow_worker6 = call i32 @slow_worker()
  %slow_worker7 = call i32 @slow_worker()
  %slow_worker8 = call i32 @slow_worker()
  %slow_worker9 = call i32 @slow_worker()
  %2 = call i32 @usleep(i32 500000)
  %time_now10 = call i64 @wyn_time_now()
  store i64 %time_now10, ptr %end, align 4
  %end11 = load i64, ptr %end, align 4
  %start12 = load i64, ptr %start, align 4
  %sub = sub i64 %end11, %start12
  store i64 %sub, ptr %duration, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %duration13 = load i64, ptr %duration, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.7, i64 %duration13)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %duration14 = load i64, ptr %duration, align 4
  %icmp = icmp slt i64 %duration14, i32 2
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr @str.9)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.11)
  ret i32 0

if.else:                                          ; preds = %entry
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %10 = call i32 (ptr, ...) @printf(ptr @nl.14)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i32 @usleep(i32)

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

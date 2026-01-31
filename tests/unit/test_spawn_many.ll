; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [23 x i8] c"Spawning 1000 tasks...\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [23 x i8] c"Spawned 1000 tasks in:\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.4 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.5 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.6 = private unnamed_addr constant [8 x i8] c"seconds\00", align 1
@fmt.7 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.8 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.9 = private unnamed_addr constant [19 x i8] c"\E2\9C\93 Spawn is fast!\00", align 1
@fmt.10 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.11 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.12 = private unnamed_addr constant [22 x i8] c"\E2\9C\97 Spawn is too slow\00", align 1
@fmt.13 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.14 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @worker(i32 %id) {
entry:
  %x = alloca i32, align 4
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  %mul = mul i32 %id2, 2
  store i32 %mul, ptr %x, align 4
  %x3 = load i32, ptr %x, align 4
  ret i32 %x3
}

define i32 @wyn_main() {
entry:
  %duration = alloca i64, align 8
  %end = alloca i64, align 8
  %i = alloca i32, align 4
  %start = alloca i64, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 1000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i2 = load i32, ptr %i, align 4
  %worker = call i32 @worker(i32 %i2)
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %i3, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %2 = call i32 @usleep(i32 1000000)
  %time_now4 = call i64 @wyn_time_now()
  store i64 %time_now4, ptr %end, align 4
  %end5 = load i64, ptr %end, align 4
  %start6 = load i64, ptr %start, align 4
  %sub = sub i64 %end5, %start6
  store i64 %sub, ptr %duration, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %duration7 = load i64, ptr %duration, align 4
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.4, i64 %duration7)
  %6 = call i32 (ptr, ...) @printf(ptr @nl.5)
  %7 = call i32 (ptr, ...) @printf(ptr @fmt.7, ptr @str.6)
  %8 = call i32 (ptr, ...) @printf(ptr @nl.8)
  %duration8 = load i64, ptr %duration, align 4
  %icmp9 = icmp slt i64 %duration8, i32 5
  br i1 %icmp9, label %if.then, label %if.else

if.then:                                          ; preds = %while.end
  %9 = call i32 (ptr, ...) @printf(ptr @fmt.10, ptr @str.9)
  %10 = call i32 (ptr, ...) @printf(ptr @nl.11)
  ret i32 0

if.else:                                          ; preds = %while.end
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.13, ptr @str.12)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.14)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

declare i32 @usleep(i32)

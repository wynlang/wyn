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

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @noop() {
entry:
  ret i32 0
}

define i32 @benchmark(i32 %n, i32 %passes) {
entry:
  %avg = alloca i32, align 4
  %wait = alloca i32, align 4
  %elapsed = alloca i64, align 8
  %i = alloca i32, align 4
  %start = alloca i64, align 8
  %max = alloca i32, align 4
  %min = alloca i32, align 4
  %sum = alloca i32, align 4
  %pass = alloca i32, align 4
  %passes2 = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  store i32 %passes, ptr %passes2, align 4
  store i32 0, ptr %pass, align 4
  store i32 0, ptr %sum, align 4
  store i32 999999999, ptr %min, align 4
  store i32 0, ptr %max, align 4
  br label %while.header

while.header:                                     ; preds = %while.end31, %entry
  %pass3 = load i32, ptr %pass, align 4
  %passes4 = load i32, ptr %passes2, align 4
  %icmp = icmp slt i32 %pass3, %passes4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header5

while.end:                                        ; preds = %while.header
  %sum36 = load i32, ptr %sum, align 4
  %passes37 = load i32, ptr %passes2, align 4
  %div = sdiv i32 %sum36, %passes37
  store i32 %div, ptr %avg, align 4
  %n38 = load i32, ptr %n1, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i32 %n38)
  %passes39 = load i32, ptr %passes2, align 4
  %1 = call i32 (ptr, ...) @printf(ptr @fmt.1, i32 %passes39)
  %min40 = load i32, ptr %min, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i32 %min40)
  %avg41 = load i32, ptr %avg, align 4
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.3, i32 %avg41)
  %max42 = load i32, ptr %max, align 4
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.4, i32 %max42)
  %avg43 = load i32, ptr %avg, align 4
  %n44 = load i32, ptr %n1, align 4
  %div45 = sdiv i32 %avg43, %n44
  %5 = call i32 (ptr, ...) @printf(ptr @fmt.5, i32 %div45)
  ret i32 0

while.header5:                                    ; preds = %while.body6, %while.body
  %i8 = load i32, ptr %i, align 4
  %n9 = load i32, ptr %n1, align 4
  %icmp10 = icmp slt i32 %i8, %n9
  br i1 %icmp10, label %while.body6, label %while.end7

while.body6:                                      ; preds = %while.header5
  call void @wyn_spawn(ptr @__spawn_noop_0, ptr null)
  %i11 = load i32, ptr %i, align 4
  %add = add i32 %i11, 1
  store i32 %add, ptr %i, align 4
  br label %while.header5

while.end7:                                       ; preds = %while.header5
  %time_now12 = call i64 @wyn_time_now()
  %start13 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now12, %start13
  store i64 %sub, ptr %elapsed, align 4
  %sum14 = load i32, ptr %sum, align 4
  %elapsed15 = load i64, ptr %elapsed, align 4
  %add16 = add i32 %sum14, i64 %elapsed15
  store i32 %add16, ptr %sum, align 4
  %elapsed17 = load i64, ptr %elapsed, align 4
  %min18 = load i32, ptr %min, align 4
  %icmp19 = icmp slt i64 %elapsed17, i32 %min18
  br i1 %icmp19, label %if.then, label %if.end

if.then:                                          ; preds = %while.end7
  %elapsed20 = load i64, ptr %elapsed, align 4
  store i64 %elapsed20, ptr %min, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %while.end7
  %elapsed21 = load i64, ptr %elapsed, align 4
  %max22 = load i32, ptr %max, align 4
  %icmp23 = icmp sgt i64 %elapsed21, i32 %max22
  br i1 %icmp23, label %if.then24, label %if.end25

if.then24:                                        ; preds = %if.end
  %elapsed26 = load i64, ptr %elapsed, align 4
  store i64 %elapsed26, ptr %max, align 4
  br label %if.end25

if.end25:                                         ; preds = %if.then24, %if.end
  %pass27 = load i32, ptr %pass, align 4
  %add28 = add i32 %pass27, 1
  store i32 %add28, ptr %pass, align 4
  store i32 0, ptr %wait, align 4
  br label %while.header29

while.header29:                                   ; preds = %while.body30, %if.end25
  %wait32 = load i32, ptr %wait, align 4
  %icmp33 = icmp slt i32 %wait32, 10000000
  br i1 %icmp33, label %while.body30, label %while.end31

while.body30:                                     ; preds = %while.header29
  %wait34 = load i32, ptr %wait, align 4
  %add35 = add i32 %wait34, 1
  store i32 %add35, ptr %wait, align 4
  br label %while.header29

while.end31:                                      ; preds = %while.header29
  br label %while.header
}

define i32 @wyn_main() {
entry:
  %benchmark = call i32 @benchmark(i32 10000, i32 10)
  %benchmark1 = call i32 @benchmark(i32 100000, i32 5)
  %benchmark2 = call i32 @benchmark(i32 1000000, i32 3)
  ret i32 0
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

define void @__spawn_noop_0(ptr %0) {
entry:
  %1 = call i32 @noop()
  ret void
}

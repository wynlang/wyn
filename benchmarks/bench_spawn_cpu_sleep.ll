; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @fib(i32 %n) {
entry:
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n2 = load i32, ptr %n1, align 4
  %icmp = icmp slt i32 %n2, 2
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %n3 = load i32, ptr %n1, align 4
  ret i32 %n3

if.end:                                           ; preds = %entry
  %n4 = load i32, ptr %n1, align 4
  %sub = sub i32 %n4, 1
  %fib = call i32 @fib(i32 %sub)
  %n5 = load i32, ptr %n1, align 4
  %sub6 = sub i32 %n5, 2
  %fib7 = call i32 @fib(i32 %sub6)
  %add = add i32 %fib, %fib7
  ret i32 %add
}

define i32 @wyn_main() {
entry:
  %elapsed = alloca i64, align 8
  %t = alloca i64, align 8
  %wait = alloca i32, align 4
  %i = alloca i32, align 4
  %start = alloca i64, align 8
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 100
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  call void @wyn_spawn(ptr @__spawn_fib_0, ptr null)
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %wait, align 4
  br label %while.header3

while.header3:                                    ; preds = %if.end, %while.end
  %wait6 = load i32, ptr %wait, align 4
  %icmp7 = icmp slt i32 %wait6, 2000000000
  br i1 %icmp7, label %while.body4, label %while.end5

while.body4:                                      ; preds = %while.header3
  %time_now8 = call i64 @wyn_time_now()
  store i64 %time_now8, ptr %t, align 4
  %t9 = load i64, ptr %t, align 4
  %start10 = load i64, ptr %start, align 4
  %sub = sub i64 %t9, %start10
  %icmp11 = icmp sgt i64 %sub, i32 2000000000
  br i1 %icmp11, label %if.then, label %if.end

while.end5:                                       ; preds = %while.header3
  %time_now14 = call i64 @wyn_time_now()
  %start15 = load i64, ptr %start, align 4
  %sub16 = sub i64 %time_now14, %start15
  store i64 %sub16, ptr %elapsed, align 4
  %elapsed17 = load i64, ptr %elapsed, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, i64 %elapsed17)
  ret i32 0

if.then:                                          ; preds = %while.body4
  store i32 2000000000, ptr %wait, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body4
  %wait12 = load i32, ptr %wait, align 4
  %add13 = add i32 %wait12, 1
  store i32 %add13, ptr %wait, align 4
  br label %while.header3
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

define void @__spawn_fib_0(ptr %0) {
entry:
  %1 = call i32 @fib(i32 25)
  ret void
}

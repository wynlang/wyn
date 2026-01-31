; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @task(i32 %id) {
entry:
  %start = alloca i64, align 8
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %time_now2 = call i64 @wyn_time_now()
  %start3 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now2, %start3
  %icmp = icmp slt i64 %sub, i32 100000000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  br label %while.header

while.end:                                        ; preds = %while.header
  %id4 = load i32, ptr %id1, align 4
  ret i32 %id4
}

define i32 @wyn_main() {
entry:
  %elapsed = alloca i64, align 8
  %start = alloca i64, align 8
  %time_now = call i64 @wyn_time_now()
  store i64 %time_now, ptr %start, align 4
  %time_now1 = call i64 @wyn_time_now()
  %start2 = load i64, ptr %start, align 4
  %sub = sub i64 %time_now1, %start2
  store i64 %sub, ptr %elapsed, align 4
  %elapsed3 = load i64, ptr %elapsed, align 4
  %icmp = icmp sgt i64 %elapsed3, i32 300000000
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

declare i64 @wyn_time_now()

declare void @wyn_spawn(ptr, ptr)

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %failures = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 50
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %pool_add = call i32 @pool_add_task(ptr @str)
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %pool_start = call void @pool_start(i32 50)
  %pool_wait = call i32 @pool_wait()
  store i32 %pool_wait, ptr %failures, align 4
  %failures3 = load i32, ptr %failures, align 4
  ret i32 %failures3
}

declare i32 @pool_add_task(ptr)

declare void @pool_start(i32)

declare i32 @pool_wait()

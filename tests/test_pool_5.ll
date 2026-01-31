; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1
@str.1 = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1
@str.2 = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1
@str.3 = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1
@str.4 = private unnamed_addr constant [8 x i8] c"sleep 1\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %failures = alloca i32, align 4
  %pool_add = call i32 @pool_add_task(ptr @str)
  %pool_add1 = call i32 @pool_add_task(ptr @str.1)
  %pool_add2 = call i32 @pool_add_task(ptr @str.2)
  %pool_add3 = call i32 @pool_add_task(ptr @str.3)
  %pool_add4 = call i32 @pool_add_task(ptr @str.4)
  %pool_start = call void @pool_start(i32 5)
  %pool_wait = call i32 @pool_wait()
  store i32 %pool_wait, ptr %failures, align 4
  %failures5 = load i32, ptr %failures, align 4
  ret i32 %failures5
}

declare i32 @pool_add_task(ptr)

declare void @pool_start(i32)

declare i32 @pool_wait()

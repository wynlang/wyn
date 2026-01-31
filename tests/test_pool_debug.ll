; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [66 x i8] c"cd /Users/aoaws/src/ao/wyn-lang/wyn && ls tests/unit/test_add.wyn\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %failures = alloca i32, align 4
  %pool_add = call i32 @pool_add_task(ptr @str)
  %pool_start = call void @pool_start(i32 1)
  %pool_wait = call i32 @pool_wait()
  store i32 %pool_wait, ptr %failures, align 4
  %failures1 = load i32, ptr %failures, align 4
  ret i32 %failures1
}

declare i32 @pool_add_task(ptr)

declare void @pool_start(i32)

declare i32 @pool_wait()

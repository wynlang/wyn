; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare noalias ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @task() {
entry:
  ret i32 42
}

define i32 @wyn_main() {
entry:
  %0 = call ptr @wyn_spawn_async(ptr @__spawn_task_0, ptr null)
  %1 = call ptr @wyn_spawn_async(ptr @__spawn_task_1, ptr null)
  %2 = call ptr @wyn_spawn_async(ptr @__spawn_task_2, ptr null)
  %3 = call i32 (ptr, ...) @printf(ptr @fmt, i32 1)
  ret i32 0
}

declare ptr @wyn_spawn_async(ptr, ptr)

define ptr @__spawn_task_0(ptr %0) {
entry:
  %1 = call i32 @task()
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_task_1(ptr %0) {
entry:
  %1 = call i32 @task()
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

define ptr @__spawn_task_2(ptr %0) {
entry:
  %1 = call i32 @task()
  %result_alloc = tail call ptr @malloc(i32 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i32))
  store i32 %1, ptr %result_alloc, align 4
  ret ptr %result_alloc
}

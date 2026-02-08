; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %y = alloca ptr, align 8
  %x = alloca ptr, align 8
  %tmp = alloca i32, align 4
  store i32 99, ptr %tmp, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %x, align 8
  %tmp1 = alloca i32, align 4
  store i32 42, ptr %tmp1, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp1, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %y, align 8
  ret i32 99
}

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_some(ptr, i64)

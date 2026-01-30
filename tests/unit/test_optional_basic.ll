; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @get_value() {
entry:
  %x = alloca ptr, align 8
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %x, align 8
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %y = alloca ptr, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %y, align 8
  %get_value = call i32 @get_value()
  ret i32 %get_value
}

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_none()

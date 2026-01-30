; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca ptr, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %x, align 8
  ret i32 0
}

declare ptr @wyn_none()

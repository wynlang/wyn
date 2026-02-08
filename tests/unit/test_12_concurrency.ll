; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @worker() {
entry:
  ret i32 1
}

define i32 @wyn_main() {
entry:
  %worker = call i32 @worker()
  ret i32 42
}

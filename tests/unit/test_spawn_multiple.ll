; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @worker1() {
entry:
  ret i32 1
}

define i32 @worker2() {
entry:
  ret i32 2
}

define i32 @worker3() {
entry:
  ret i32 3
}

define i32 @wyn_main() {
entry:
  ret i32 0
}

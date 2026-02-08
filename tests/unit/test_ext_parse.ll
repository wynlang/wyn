; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @distance(i32 %p) {
entry:
  %p1 = alloca i32, align 4
  store i32 %p, ptr %p1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  ret i32 0
}

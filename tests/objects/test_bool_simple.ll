; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %f = alloca i1, align 1
  %t = alloca i1, align 1
  store i1 true, ptr %t, align 1
  store i1 false, ptr %f, align 1
  %t1 = load i1, ptr %t, align 1
  %f2 = load i1, ptr %f, align 1
  ret i32 0
}

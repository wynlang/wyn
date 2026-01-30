; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %b = alloca i1, align 1
  %a = alloca i1, align 1
  store i1 true, ptr %a, align 1
  store i1 false, ptr %b, align 1
  %a1 = load i1, ptr %a, align 1
  %b2 = load i1, ptr %b, align 1
  ret i32 0
}

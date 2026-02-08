; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %result1 = load i32, ptr %result, align 4
  ret i32 %result1
}

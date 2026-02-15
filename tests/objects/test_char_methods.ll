; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %c = alloca i32, align 4
  %c1 = load i32, ptr %c, align 4
  %c2 = load i32, ptr %c, align 4
  %not = xor i32 %c2, -1
  %c3 = load i32, ptr %c, align 4
  %not4 = xor i32 %c3, -1
  ret i32 0
}

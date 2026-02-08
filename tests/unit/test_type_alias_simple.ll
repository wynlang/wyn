; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %id = alloca i32, align 4
  store i32 42, ptr %id, align 4
  %id1 = load i32, ptr %id, align 4
  ret i32 %id1
}

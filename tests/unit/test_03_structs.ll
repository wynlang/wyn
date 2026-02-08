; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %p = alloca %Point, align 8
  store %Point { i32 42, i32 10 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0
  ret i32 %field_val
}

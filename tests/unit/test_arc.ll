; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Box = type { i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %b = alloca %Box, align 8
  store %Box { i32 42 }, ptr %b, align 4
  %b1 = load %Box, ptr %b, align 4
  %field_val = extractvalue %Box %b1, 0
  ret i32 %field_val
}

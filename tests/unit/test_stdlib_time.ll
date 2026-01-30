; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @now() {
entry:
  ret i32 1705147200
}

define i32 @year() {
entry:
  ret i32 2026
}

define i32 @wyn_main() {
entry:
  %y = alloca i32, align 4
  %timestamp = alloca i32, align 4
  %now = call i32 @now()
  store i32 %now, ptr %timestamp, align 4
  %year = call i32 @year()
  store i32 %year, ptr %y, align 4
  %y1 = load i32, ptr %y, align 4
  %sub = sub i32 %y1, 2000
  ret i32 %sub
}

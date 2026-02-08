; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %dist = alloca i32, align 4
  %score = alloca i32, align 4
  %user = alloca i32, align 4
  store i32 100, ptr %user, align 4
  store i32 95, ptr %score, align 4
  store i32 50, ptr %dist, align 4
  %user1 = load i32, ptr %user, align 4
  %score2 = load i32, ptr %score, align 4
  %add = add i32 %user1, %score2
  %dist3 = load i32, ptr %dist, align 4
  %add4 = add i32 %add, %dist3
  ret i32 %add4
}

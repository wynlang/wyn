; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@RED = internal constant i32 0
@GREEN = internal constant i32 1
@BLUE = internal constant i32 2

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %c = alloca i32, align 4
  %RED = load i32, ptr @RED, align 4
  store i32 %RED, ptr %c, align 4
  %c1 = load i32, ptr %c, align 4
  br label %match_case4

match_end:                                        ; No predecessors!
  ret i32 0

match_case:                                       ; No predecessors!
  ret i32 1

match_case2:                                      ; No predecessors!
  ret i32 2

match_case3:                                      ; No predecessors!
  ret i32 3

match_case4:                                      ; preds = %entry
  ret i32 0
}

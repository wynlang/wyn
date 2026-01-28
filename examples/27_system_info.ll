; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %basename = alloca i32, align 4
  %cwd = alloca i32, align 4
  %i = alloca i32, align 4
  %args = alloca i32, align 4
  %formatted = alloca i32, align 4
  %now = alloca i32, align 4
  %set_result = alloca i32, align 4
  %user = alloca i32, align 4
  %home = alloca i32, align 4
  %path = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %entry
  ret i32 0

while.body:                                       ; No predecessors!

while.end:                                        ; No predecessors!
}

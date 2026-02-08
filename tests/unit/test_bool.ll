; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %flag = alloca i1, align 1
  store i1 true, ptr %flag, align 1
  %flag1 = load i1, ptr %flag, align 1
  br i1 %flag1, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 42

if.end:                                           ; preds = %entry
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %a = alloca i32, align 4
  %wyn_abs = call i32 @wyn_abs(i32 -42)
  store i32 %wyn_abs, ptr %a, align 4
  %a1 = load i32, ptr %a, align 4
  %icmp = icmp ne i32 %a1, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 99
}

declare i32 @wyn_abs(i32)

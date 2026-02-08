; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  store i32 %x1, ptr %y, align 4
  store i32 100, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %icmp = icmp ne i32 %y2, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %compute_value = call i32 @compute_value()
  store i32 %compute_value, ptr %result, align 4
  %result1 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result1, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @compute_value() {
entry:
  ret i32 42
}

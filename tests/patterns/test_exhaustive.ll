; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result2 = alloca i32, align 4
  %c2 = alloca i32, align 4
  %result = alloca i32, align 4
  %c = alloca i32, align 4
  %result1 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result1, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %result22 = load i32, ptr %result2, align 4
  %icmp3 = icmp ne i32 %result22, 2
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  ret i32 0
}

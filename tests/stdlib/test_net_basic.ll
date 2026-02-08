; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %close_result = alloca i32, align 4
  %server = alloca i32, align 4
  %server1 = load i32, ptr %server, align 4
  %icmp = icmp eq i32 %server1, -1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %close_result2 = load i32, ptr %close_result, align 4
  %icmp3 = icmp eq i32 %close_result2, 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  ret i32 0
}

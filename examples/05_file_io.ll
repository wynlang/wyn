; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %exists = alloca i32, align 4
  %content = alloca i32, align 4
  %write_result = alloca i32, align 4
  %write_result1 = load i32, ptr %write_result, align 4
  %icmp = icmp ne i32 %write_result1, i32 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %exists2 = load i32, ptr %exists, align 4
  %icmp3 = icmp eq i32 %exists2, i32 1
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  br label %if.end5

if.end5:                                          ; preds = %if.then4, %if.end
  ret i32 0
}

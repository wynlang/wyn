; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@PENDING = internal constant i32 0
@RUNNING = internal constant i32 1
@DONE = internal constant i32 2

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %s = alloca i32, align 4
  %PENDING = load i32, ptr @PENDING, align 4
  store i32 %PENDING, ptr %s, align 4
  %s1 = load i32, ptr %s, align 4
  %match.result = alloca i32, align 4
  %PENDING3 = load i32, ptr @PENDING, align 4
  %match.cmp = icmp eq i32 %s1, %PENDING3
  br i1 %match.cmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm2, %match.next, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result5 = load i32, ptr %result, align 4
  ret i32 %result5

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  %RUNNING = load i32, ptr @RUNNING, align 4
  %match.cmp4 = icmp eq i32 %s1, %RUNNING
  br i1 %match.cmp4, label %match.arm2, label %match.end

match.arm2:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end
}

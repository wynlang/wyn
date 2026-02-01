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
  %s = alloca i32, align 4
  %s1 = load i32, ptr %s, align 4
  %match.result = alloca i32, align 4
  br label %match.arm

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result3 = load i32, ptr %result, align 4
  ret i32 %result3

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; No predecessors!
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end
}

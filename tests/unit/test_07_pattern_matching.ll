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
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %match.result = alloca i32, align 4
  %match.cmp = icmp eq i32 %x1, 5
  br i1 %match.cmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result3 = load i32, ptr %result, align 4
  ret i32 %result3

match.arm:                                        ; preds = %entry
  store i32 55, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  store i32 0, ptr %match.result, align 4
  br label %match.end
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca i32, align 4
  store i32 0, ptr %x, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %x1 = load i32, ptr %x, align 4
  %icmp = icmp slt i32 %x1, 5
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %x2 = load i32, ptr %x, align 4
  %add = add i32 %x2, 1
  store i32 %add, ptr %x, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %x3 = load i32, ptr %x, align 4
  ret i32 %x3
}

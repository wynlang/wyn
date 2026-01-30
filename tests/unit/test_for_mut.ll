; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %i = alloca i32, align 4
  %sum = alloca i32, align 4
  store i32 0, ptr %sum, align 4
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 5
  br i1 %icmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %sum2 = load i32, ptr %sum, align 4
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %sum2, %i3
  store i32 %add, ptr %sum, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %i4 = load i32, ptr %i, align 4
  %add5 = add i32 %i4, 1
  store i32 %add5, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %for.header
  %sum6 = load i32, ptr %sum, align 4
  ret i32 %sum6
}

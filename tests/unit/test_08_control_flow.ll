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
  br i1 true, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  store i32 10, ptr %sum, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %sum1 = load i32, ptr %sum, align 4
  %icmp = icmp slt i32 %sum1, 20
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %sum2 = load i32, ptr %sum, align 4
  %add = add i32 %sum2, 5
  store i32 %add, ptr %sum, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %while.end
  %i3 = load i32, ptr %i, align 4
  %icmp4 = icmp slt i32 %i3, 3
  br i1 %icmp4, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %i5 = load i32, ptr %i, align 4
  %icmp6 = icmp eq i32 %i5, 1
  br i1 %icmp6, label %if.then7, label %if.end8

for.inc:                                          ; preds = %if.end8
  %i9 = load i32, ptr %i, align 4
  %add10 = add i32 %i9, 1
  store i32 %add10, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %if.then7, %for.header
  ret i32 42

if.then7:                                         ; preds = %for.body
  br label %for.end

if.end8:                                          ; preds = %for.body
  br label %for.inc
}

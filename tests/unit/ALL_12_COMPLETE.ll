; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @add(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %add = add i32 %a3, %b4
  ret i32 %add
}

define i32 @identity(i32 %val) {
entry:
  %val1 = alloca i32, align 4
  store i32 %val, ptr %val1, align 4
  %val2 = load i32, ptr %val1, align 4
  ret i32 %val2
}

define i32 @distance(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %add = add i32 %a3, %b4
  ret i32 %add
}

define i32 @worker() {
entry:
  ret i32 1
}

define i32 @wyn_main() {
entry:
  %ext_result = alloca i32, align 4
  %val = alloca i32, align 4
  %id = alloca i32, align 4
  %i = alloca i32, align 4
  %temp = alloca i32, align 4
  %result = alloca i32, align 4
  %maybe = alloca ptr, align 8
  %success = alloca ptr, align 8
  %arr = alloca ptr, align 8
  %s = alloca i32, align 4
  %p = alloca i32, align 4
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 20, ptr %y, align 4
  store i32 30, ptr %y, align 4
  %add = call i32 @add(i32 10, i32 20)
  store i32 %add, ptr %sum, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %success, align 8
  %tmp3 = alloca i32, align 4
  store i32 42, ptr %tmp3, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp3, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %maybe, align 8
  %result4 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result4, 88
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  store i32 1, ptr %temp, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %y5 = load i32, ptr %y, align 4
  %icmp6 = icmp slt i32 %y5, 35
  br i1 %icmp6, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %y7 = load i32, ptr %y, align 4
  %add8 = add i32 %y7, 1
  store i32 %add8, ptr %y, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %while.end
  %i9 = load i32, ptr %i, align 4
  %icmp10 = icmp slt i32 %i9, 5
  br i1 %icmp10, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %i11 = load i32, ptr %i, align 4
  %icmp12 = icmp eq i32 %i11, 2
  br i1 %icmp12, label %if.then13, label %if.end14

for.inc:                                          ; preds = %if.end14
  %i15 = load i32, ptr %i, align 4
  %add16 = add i32 %i15, 1
  store i32 %add16, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %if.then13, %for.header
  store i32 123, ptr %id, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %val, align 4
  %result17 = load i32, ptr %result, align 4
  ret i32 %result17

if.then13:                                        ; preds = %for.body
  br label %for.end

if.end14:                                         ; preds = %for.body
  br label %for.inc
}

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_some(ptr, i64)

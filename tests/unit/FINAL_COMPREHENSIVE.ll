; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @identity(i32 %val) {
entry:
  %val1 = alloca i32, align 4
  store i32 %val, ptr %val1, align 4
  %val2 = load i32, ptr %val1, align 4
  ret i32 %val2
}

define i32 @wyn_main() {
entry:
  %nothing = alloca ptr, align 8
  %maybe = alloca ptr, align 8
  %failure = alloca ptr, align 8
  %success = alloca ptr, align 8
  %result = alloca i32, align 4
  %i = alloca i32, align 4
  %total = alloca i32, align 4
  %counter = alloca i32, align 4
  %temp = alloca i32, align 4
  %val = alloca i32, align 4
  %id = alloca i32, align 4
  %status = alloca i32, align 4
  %arr = alloca ptr, align 8
  %p = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store i32 30, ptr %y, align 4
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  store i32 123, ptr %id, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %val, align 4
  %val3 = load i32, ptr %val, align 4
  %icmp = icmp eq i32 %val3, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  store i32 1, ptr %temp, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  store i32 0, ptr %counter, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %counter4 = load i32, ptr %counter, align 4
  %icmp5 = icmp slt i32 %counter4, 3
  br i1 %icmp5, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter6 = load i32, ptr %counter, align 4
  %add = add i32 %counter6, 1
  store i32 %add, ptr %counter, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %total, align 4
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %while.end
  %i7 = load i32, ptr %i, align 4
  %icmp8 = icmp slt i32 %i7, 5
  br i1 %icmp8, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %total9 = load i32, ptr %total, align 4
  %i10 = load i32, ptr %i, align 4
  %add11 = add i32 %total9, %i10
  store i32 %add11, ptr %total, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %i12 = load i32, ptr %i, align 4
  %add13 = add i32 %i12, 1
  store i32 %add13, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %for.header
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %success, align 8
  %tmp14 = alloca i32, align 4
  store i32 1, ptr %tmp14, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp14, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_err, ptr %failure, align 8
  %tmp15 = alloca i32, align 4
  store i32 42, ptr %tmp15, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp15, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %maybe, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %nothing, align 8
  ret i32 88
}

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_err(ptr, i64)

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_none()

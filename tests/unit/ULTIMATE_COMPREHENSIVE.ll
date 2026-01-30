; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1

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
  %quot = alloca i32, align 4
  %prod = alloca i32, align 4
  %diff = alloca i32, align 4
  %sum = alloca i32, align 4
  %pi = alloca double, align 8
  %other = alloca i1, align 1
  %flag = alloca i1, align 1
  %msg = alloca ptr, align 8
  %nothing = alloca ptr, align 8
  %maybe = alloca ptr, align 8
  %failure = alloca ptr, align 8
  %success = alloca ptr, align 8
  %final_val = alloca i32, align 4
  %result = alloca i32, align 4
  %i = alloca i32, align 4
  %total = alloca i32, align 4
  %counter = alloca i32, align 4
  %temp4 = alloca i32, align 4
  %temp = alloca i32, align 4
  %val = alloca i32, align 4
  %score = alloca i32, align 4
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
  store i32 456, ptr %score, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %val, align 4
  %val3 = load i32, ptr %val, align 4
  %icmp = icmp eq i32 %val3, 42
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 1, ptr %temp, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  store i32 0, ptr %temp4, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  store i32 0, ptr %counter, align 4
  br label %while.header

while.header:                                     ; preds = %if.end11, %if.end
  %counter5 = load i32, ptr %counter, align 4
  %icmp6 = icmp slt i32 %counter5, 10
  br i1 %icmp6, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter7 = load i32, ptr %counter, align 4
  %add = add i32 %counter7, 1
  store i32 %add, ptr %counter, align 4
  %counter8 = load i32, ptr %counter, align 4
  %icmp9 = icmp eq i32 %counter8, 5
  br i1 %icmp9, label %if.then10, label %if.end11

while.end:                                        ; preds = %if.then10, %while.header
  store i32 0, ptr %total, align 4
  store i32 0, ptr %i, align 4
  br label %for.header

if.then10:                                        ; preds = %while.body
  br label %while.end

if.end11:                                         ; preds = %while.body
  br label %while.header

for.header:                                       ; preds = %for.inc, %while.end
  %i12 = load i32, ptr %i, align 4
  %icmp13 = icmp slt i32 %i12, 10
  br i1 %icmp13, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %i14 = load i32, ptr %i, align 4
  %icmp15 = icmp eq i32 %i14, 5
  br i1 %icmp15, label %if.then16, label %if.end17

for.inc:                                          ; preds = %if.end17, %if.then16
  %i21 = load i32, ptr %i, align 4
  %add22 = add i32 %i21, 1
  store i32 %add22, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %for.header
  store i32 0, ptr %final_val, align 4
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %success, align 8
  %tmp23 = alloca i32, align 4
  store i32 1, ptr %tmp23, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp23, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_err, ptr %failure, align 8
  %tmp24 = alloca i32, align 4
  store i32 42, ptr %tmp24, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp24, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %maybe, align 8
  %none = call ptr @wyn_none()
  store ptr %none, ptr %nothing, align 8
  store ptr @str, ptr %msg, align 8
  store i1 true, ptr %flag, align 1
  store i1 false, ptr %other, align 1
  %flag25 = load i1, ptr %flag, align 1
  %other26 = load i1, ptr %other, align 1
  %x27 = load i32, ptr %x, align 4
  %icmp28 = icmp sgt i32 %x27, 5
  %x29 = load i32, ptr %x, align 4
  %icmp30 = icmp slt i32 %x29, 15
  %x31 = load i32, ptr %x, align 4
  %icmp32 = icmp sge i32 %x31, 10
  %x33 = load i32, ptr %x, align 4
  %icmp34 = icmp sle i32 %x33, 10
  %x35 = load i32, ptr %x, align 4
  %icmp36 = icmp ne i32 %x35, 5
  store double 3.140000e+00, ptr %pi, align 8
  store i32 30, ptr %sum, align 4
  store i32 20, ptr %diff, align 4
  store i32 30, ptr %prod, align 4
  store i32 5, ptr %quot, align 4
  ret i32 77

if.then16:                                        ; preds = %for.body
  br label %for.inc

if.end17:                                         ; preds = %for.body
  %total18 = load i32, ptr %total, align 4
  %i19 = load i32, ptr %i, align 4
  %add20 = add i32 %total18, %i19
  store i32 %add20, ptr %total, align 4
  br label %for.inc
}

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_err(ptr, i64)

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_none()

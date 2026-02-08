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
  %result = alloca i32, align 4
  %i = alloca i32, align 4
  %total = alloca i32, align 4
  %counter = alloca i32, align 4
  %temp5 = alloca i32, align 4
  %temp = alloca i32, align 4
  %val = alloca i32, align 4
  %status = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %sum = alloca i32, align 4
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
  %arr3 = load ptr, ptr %arr, align 8
  %array_element_ptr = getelementptr [0 x i32], ptr %arr3, i32 0, i32 0
  %array_element = load i32, ptr %array_element_ptr, align 4
  store i32 %array_element, ptr %first, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %val, align 4
  %val4 = load i32, ptr %val, align 4
  %icmp = icmp eq i32 %val4, 42
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 1, ptr %temp, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  store i32 0, ptr %temp5, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  store i32 0, ptr %counter, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %counter6 = load i32, ptr %counter, align 4
  %icmp7 = icmp slt i32 %counter6, 3
  br i1 %icmp7, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter8 = load i32, ptr %counter, align 4
  %add = add i32 %counter8, 1
  store i32 %add, ptr %counter, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %total, align 4
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %while.end
  %i9 = load i32, ptr %i, align 4
  %icmp10 = icmp slt i32 %i9, 5
  br i1 %icmp10, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %total11 = load i32, ptr %total, align 4
  %i12 = load i32, ptr %i, align 4
  %add13 = add i32 %total11, %i12
  store i32 %add13, ptr %total, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %i14 = load i32, ptr %i, align 4
  %add15 = add i32 %i14, 1
  store i32 %add15, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %for.header
  ret i32 0
}

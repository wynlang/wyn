; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Point = type { i32, i32 }

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

define i32 @sum(i32 %a, i32 %b) {
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
  %ext = alloca i32, align 4
  %gen = alloca i32, align 4
  %id = alloca i32, align 4
  %i = alloca i32, align 4
  %temp = alloca i32, align 4
  %counter = alloca i32, align 4
  %matched = alloca i32, align 4
  %some_val = alloca ptr, align 8
  %ok_val = alloca ptr, align 8
  %arr = alloca ptr, align 8
  %s = alloca i32, align 4
  %p = alloca %Point, align 8
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store i32 30, ptr %y, align 4
  %add = call i32 @add(i32 10, i32 20)
  store i32 %add, ptr %sum, align 4
  store %Point { i32 3, i32 4 }, ptr %p, align 4
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
  store ptr %wyn_ok, ptr %ok_val, align 8
  %tmp3 = alloca i32, align 4
  store i32 42, ptr %tmp3, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp3, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %some_val, align 8
  %p4 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p4, 0
  %match.result = alloca i32, align 4
  %match.cmp = icmp eq i32 %field_val, 3
  br i1 %match.cmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm5, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %matched, align 4
  store i32 0, ptr %counter, align 4
  %matched6 = load i32, ptr %matched, align 4
  %icmp = icmp eq i32 %matched6, 77
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 77, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  br label %match.arm5

match.arm5:                                       ; preds = %match.next
  store i32 0, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  store i32 1, ptr %temp, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %match.end
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %counter7 = load i32, ptr %counter, align 4
  %icmp8 = icmp slt i32 %counter7, 3
  br i1 %icmp8, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter9 = load i32, ptr %counter, align 4
  %add10 = add i32 %counter9, 1
  store i32 %add10, ptr %counter, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  store i32 0, ptr %i, align 4
  br label %for.header

for.header:                                       ; preds = %for.inc, %while.end
  %i11 = load i32, ptr %i, align 4
  %icmp12 = icmp slt i32 %i11, 5
  br i1 %icmp12, label %for.body, label %for.end

for.body:                                         ; preds = %for.header
  %i13 = load i32, ptr %i, align 4
  %icmp14 = icmp eq i32 %i13, 2
  br i1 %icmp14, label %if.then15, label %if.end16

for.inc:                                          ; preds = %if.end16
  %i17 = load i32, ptr %i, align 4
  %add18 = add i32 %i17, 1
  store i32 %add18, ptr %i, align 4
  br label %for.header

for.end:                                          ; preds = %if.then15, %for.header
  store i32 123, ptr %id, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %gen, align 4
  %worker = call i32 @worker()
  %matched19 = load i32, ptr %matched, align 4
  ret i32 %matched19

if.then15:                                        ; preds = %for.body
  br label %for.end

if.end16:                                         ; preds = %for.body
  br label %for.inc
}

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_some(ptr, i64)

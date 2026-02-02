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

define i32 @generic_identity(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  ret i32 %x2
}

define i32 @wyn_main() {
entry:
  %gen_result = alloca i32, align 4
  %i = alloca i32, align 4
  %counter = alloca i32, align 4
  %result = alloca i32, align 4
  %first = alloca i32, align 4
  %arr = alloca ptr, align 8
  %status = alloca i32, align 4
  %px = alloca i32, align 4
  %p = alloca %Point, align 8
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store i32 30, ptr %y, align 4
  %add = call i32 @add(i32 5, i32 7)
  store i32 %add, ptr %sum, align 4
  store %Point { i32 3, i32 4 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0
  store i32 %field_val, ptr %px, align 4
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr2 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr5, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr6 = load ptr, ptr %arr, align 8
  %array_element_ptr = getelementptr [0 x i32], ptr %arr6, i32 0, i32 0
  %array_element = load i32, ptr %array_element_ptr, align 4
  store i32 %array_element, ptr %first, align 4
  %status7 = load i32, ptr %status, align 4
  %match.result = alloca i32, align 4
  br label %match.arm

match.end:                                        ; preds = %match.arm10, %match.arm8, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  store i32 0, ptr %counter, align 4
  %result11 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result11, 42
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; No predecessors!
  br label %match.arm8

match.arm8:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end

match.next9:                                      ; No predecessors!
  br label %match.arm10

match.arm10:                                      ; preds = %match.next9
  store i32 42, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  %counter12 = load i32, ptr %counter, align 4
  %add13 = add i32 %counter12, 1
  store i32 %add13, ptr %counter, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %match.end
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %if.end
  %i14 = load i32, ptr %i, align 4
  %icmp15 = icmp slt i32 %i14, 3
  br i1 %icmp15, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %counter16 = load i32, ptr %counter, align 4
  %add17 = add i32 %counter16, 1
  store i32 %add17, ptr %counter, align 4
  %i18 = load i32, ptr %i, align 4
  %add19 = add i32 %i18, 1
  store i32 %add19, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %generic_identity = call i32 @generic_identity(i32 100)
  store i32 %generic_identity, ptr %gen_result, align 4
  %x20 = load i32, ptr %x, align 4
  %y21 = load i32, ptr %y, align 4
  %add22 = add i32 %x20, %y21
  %sum23 = load i32, ptr %sum, align 4
  %add24 = add i32 %add22, %sum23
  %px25 = load i32, ptr %px, align 4
  %add26 = add i32 %add24, %px25
  %result27 = load i32, ptr %result, align 4
  %add28 = add i32 %add26, %result27
  %counter29 = load i32, ptr %counter, align 4
  %add30 = add i32 %add28, %counter29
  %gen_result31 = load i32, ptr %gen_result, align 4
  %add32 = add i32 %add30, %gen_result31
  ret i32 %add32
}

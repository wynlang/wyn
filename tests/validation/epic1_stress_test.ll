; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Data.0 = type { i32, i32 }
%Data = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @process(i32 %d) {
entry:
  %d1 = alloca i32, align 4
  store i32 %d, ptr %d1, align 4
  %d2 = load i32, ptr %d1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %total = alloca i32, align 4
  %count = alloca i32, align 4
  %sum = alloca i32, align 4
  %arr = alloca ptr, align 8
  %d2 = alloca %Data.0, align 8
  %result = alloca i32, align 4
  %d1 = alloca %Data, align 8
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %y, align 4
  store %Data { i32 5, i32 1 }, ptr %d1, align 4
  %d11 = load %Data, ptr %d1, align 4
  %process = call i32 @process(%Data %d11)
  store i32 %process, ptr %result, align 4
  %result2 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result2, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  store %Data.0 { i32 15, i32 0 }, ptr %d2, align 4
  %d23 = load %Data.0, ptr %d2, align 4
  %d24 = load %Data.0, ptr %d2, align 4
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr5 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr6, align 4
  %element_ptr7 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr7, align 4
  %element_ptr8 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr8, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr9 = load ptr, ptr %arr, align 8
  %array_element_ptr = getelementptr [0 x i32], ptr %arr9, i32 0, i32 0
  %array_element = load i32, ptr %array_element_ptr, align 4
  %icmp10 = icmp ne i32 %array_element, 1
  br i1 %icmp10, label %if.then11, label %if.end12

if.then11:                                        ; preds = %if.end
  ret i32 4

if.end12:                                         ; preds = %if.end
  %arr13 = load ptr, ptr %arr, align 8
  %array_element_ptr14 = getelementptr [0 x i32], ptr %arr13, i32 0, i32 4
  %array_element15 = load i32, ptr %array_element_ptr14, align 4
  %icmp16 = icmp ne i32 %array_element15, 5
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end12
  ret i32 5

if.end18:                                         ; preds = %if.end12
  store i32 0, ptr %sum, align 4
  %d119 = load %Data, ptr %d1, align 4
  %field_val = extractvalue %Data %d119, 1
  %icmp20 = icmp eq i32 %field_val, 1
  br i1 %icmp20, label %if.then21, label %if.end22

if.then21:                                        ; preds = %if.end18
  %sum23 = load i32, ptr %sum, align 4
  %d124 = load %Data, ptr %d1, align 4
  %field_val25 = extractvalue %Data %d124, 0
  %add = add i32 %sum23, %field_val25
  store i32 %add, ptr %sum, align 4
  br label %if.end22

if.end22:                                         ; preds = %if.then21, %if.end18
  %sum26 = load i32, ptr %sum, align 4
  %icmp27 = icmp ne i32 %sum26, 5
  br i1 %icmp27, label %if.then28, label %if.end29

if.then28:                                        ; preds = %if.end22
  ret i32 6

if.end29:                                         ; preds = %if.end22
  store i32 0, ptr %count, align 4
  br label %while.header

while.header:                                     ; preds = %if.end29
  %count30 = load i32, ptr %count, align 4
  %d231 = load %Data.0, ptr %d2, align 4
  %count32 = load i32, ptr %count, align 4
  %icmp33 = icmp ne i32 %count32, 15
  br i1 %icmp33, label %if.then34, label %if.end35

while.body:                                       ; No predecessors!

while.end:                                        ; No predecessors!

if.then34:                                        ; preds = %while.header
  ret i32 7

if.end35:                                         ; preds = %while.header
  %d136 = load %Data, ptr %d1, align 4
  %field_val37 = extractvalue %Data %d136, 0
  %d238 = load %Data.0, ptr %d2, align 4
  %arr39 = load ptr, ptr %arr, align 8
  %array_element_ptr40 = getelementptr [0 x i32], ptr %arr39, i32 0, i32 2
  %array_element41 = load i32, ptr %array_element_ptr40, align 4
  %total42 = load i32, ptr %total, align 4
  %icmp43 = icmp ne i32 %total42, 23
  br i1 %icmp43, label %if.then44, label %if.end45

if.then44:                                        ; preds = %if.end35
  ret i32 8

if.end45:                                         ; preds = %if.end35
  ret i32 0
}

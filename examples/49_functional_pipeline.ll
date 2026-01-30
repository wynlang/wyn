; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [40 x i8] c"=== Functional Programming Pipeline ===\00", align 1
@str.1 = private unnamed_addr constant [10 x i8] c"Original:\00", align 1
@str.2 = private unnamed_addr constant [30 x i8] c"After filter (positive only):\00", align 1
@str.3 = private unnamed_addr constant [21 x i8] c"After map (tripled):\00", align 1
@str.4 = private unnamed_addr constant [20 x i8] c"After reduce (sum):\00", align 1
@str.5 = private unnamed_addr constant [31 x i8] c"\E2\9C\93 Functional pipeline works!\00", align 1
@str.6 = private unnamed_addr constant [20 x i8] c"\E2\9C\97 Pipeline failed\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @is_positive(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %icmp = icmp sgt i32 %x2, 0
  ret i1 %icmp
}

define i32 @triple(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, 3
  ret i32 %mul
}

define i32 @sum(i32 %acc, i32 %x) {
entry:
  %x2 = alloca i32, align 4
  %acc1 = alloca i32, align 4
  store i32 %acc, ptr %acc1, align 4
  store i32 %x, ptr %x2, align 4
  %acc3 = load i32, ptr %acc1, align 4
  %x4 = load i32, ptr %x2, align 4
  %add = add i32 %acc3, %x4
  ret i32 %add
}

define i32 @wyn_main() {
entry:
  %total = alloca i32, align 4
  %first_tripled = alloca i32, align 4
  %tripled = alloca i32, align 4
  %positives = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %print_string_call = call void @print_string(ptr @str)
  %array_literal = alloca [8 x i32], align 4
  %element_ptr = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 0
  store i32 -2, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 1
  store i32 -1, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 2
  store i32 0, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 3
  store i32 1, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 4
  store i32 2, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 5
  store i32 3, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 6
  store i32 4, ptr %element_ptr6, align 4
  %element_ptr7 = getelementptr [8 x i32], ptr %array_literal, i32 0, i32 7
  store i32 5, ptr %element_ptr7, align 4
  store ptr %array_literal, ptr %numbers, align 8
  %print_string_call8 = call void @print_string(ptr @str.1)
  %print_string_call9 = call void @print_string(ptr @str.2)
  %print_string_call10 = call void @print_string(ptr @str.3)
  %first_tripled11 = load i32, ptr %first_tripled, align 4
  %print_call = call void @print(i32 %first_tripled11)
  %print_string_call12 = call void @print_string(ptr @str.4)
  %total13 = load i32, ptr %total, align 4
  %print_call14 = call void @print(i32 %total13)
  %total15 = load i32, ptr %total, align 4
  %icmp = icmp eq i32 %total15, 45
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %print_string_call16 = call void @print_string(ptr @str.5)
  ret i32 0

if.else:                                          ; preds = %entry
  %print_string_call17 = call void @print_string(ptr @str.6)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

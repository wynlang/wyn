; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [37 x i8] c"=== Performance Verification ===\\n\\n\00", align 1
@str.1 = private unnamed_addr constant [23 x i8] c"Test 1: 1,000 spawns\\n\00", align 1
@str.2 = private unnamed_addr constant [29 x i8] c"\E2\9C\93 Created 1,000 spawns\\n\\n\00", align 1
@str.3 = private unnamed_addr constant [23 x i8] c"Test 2: 5,000 spawns\\n\00", align 1
@str.4 = private unnamed_addr constant [29 x i8] c"\E2\9C\93 Created 5,000 spawns\\n\\n\00", align 1
@str.5 = private unnamed_addr constant [24 x i8] c"Test 3: 10,000 spawns\\n\00", align 1
@str.6 = private unnamed_addr constant [30 x i8] c"\E2\9C\93 Created 10,000 spawns\\n\\n\00", align 1
@str.7 = private unnamed_addr constant [32 x i8] c"All performance tests passed!\\n\00", align 1
@str.8 = private unnamed_addr constant [40 x i8] c"Run with 'time' to measure throughput\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @empty_worker() {
entry:
  %x = alloca i32, align 4
  store i32 1, ptr %x, align 4
  ret void
}

define void @wyn_main() {
entry:
  %i = alloca i32, align 4
  %print_string_call = call void @print_string(ptr @str)
  %print_string_call1 = call void @print_string(ptr @str.1)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i2 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i2, 1000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %i3, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %print_string_call4 = call void @print_string(ptr @str.2)
  %print_string_call5 = call void @print_string(ptr @str.3)
  store i32 0, ptr %i, align 4
  br label %while.header6

while.header6:                                    ; preds = %while.body7, %while.end
  %i9 = load i32, ptr %i, align 4
  %icmp10 = icmp slt i32 %i9, 5000
  br i1 %icmp10, label %while.body7, label %while.end8

while.body7:                                      ; preds = %while.header6
  %i11 = load i32, ptr %i, align 4
  %add12 = add i32 %i11, 1
  store i32 %add12, ptr %i, align 4
  br label %while.header6

while.end8:                                       ; preds = %while.header6
  %print_string_call13 = call void @print_string(ptr @str.4)
  %print_string_call14 = call void @print_string(ptr @str.5)
  store i32 0, ptr %i, align 4
  br label %while.header15

while.header15:                                   ; preds = %while.body16, %while.end8
  %i18 = load i32, ptr %i, align 4
  %icmp19 = icmp slt i32 %i18, 10000
  br i1 %icmp19, label %while.body16, label %while.end17

while.body16:                                     ; preds = %while.header15
  %i20 = load i32, ptr %i, align 4
  %add21 = add i32 %i20, 1
  store i32 %add21, ptr %i, align 4
  br label %while.header15

while.end17:                                      ; preds = %while.header15
  %print_string_call22 = call void @print_string(ptr @str.6)
  %print_string_call23 = call void @print_string(ptr @str.7)
  %print_string_call24 = call void @print_string(ptr @str.8)
  ret i32 0
}

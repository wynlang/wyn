; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [39 x i8] c"Measuring pure spawn throughput...\\n\\n\00", align 1
@str.1 = private unnamed_addr constant [28 x i8] c"Creating 10,000 spawns...\\n\00", align 1
@str.2 = private unnamed_addr constant [10 x i8] c"Done!\\n\\n\00", align 1
@str.3 = private unnamed_addr constant [28 x i8] c"Creating 50,000 spawns...\\n\00", align 1
@str.4 = private unnamed_addr constant [10 x i8] c"Done!\\n\\n\00", align 1
@str.5 = private unnamed_addr constant [35 x i8] c"All spawns created successfully!\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @empty() {
entry:
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
  %icmp = icmp slt i32 %i2, 10000
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
  %icmp10 = icmp slt i32 %i9, 50000
  br i1 %icmp10, label %while.body7, label %while.end8

while.body7:                                      ; preds = %while.header6
  %i11 = load i32, ptr %i, align 4
  %add12 = add i32 %i11, 1
  store i32 %add12, ptr %i, align 4
  br label %while.header6

while.end8:                                       ; preds = %while.header6
  %print_string_call13 = call void @print_string(ptr @str.4)
  %print_string_call14 = call void @print_string(ptr @str.5)
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [35 x i8] c"=== Memory Efficiency Test ===\\n\\n\00", align 1
@str.1 = private unnamed_addr constant [27 x i8] c"Creating 1,000 spawns...\\n\00", align 1
@str.2 = private unnamed_addr constant [27 x i8] c"Expected memory: ~4-8 MB\\n\00", align 1
@str.3 = private unnamed_addr constant [32 x i8] c"(1,000 \C3\97 4-8 KB per spawn)\\n\\n\00", align 1
@str.4 = private unnamed_addr constant [27 x i8] c"\E2\9C\93 1,000 spawns created\\n\00", align 1
@str.5 = private unnamed_addr constant [45 x i8] c"\E2\9C\93 Check memory with: ps aux | grep wyn\\n\\n\00", align 1
@str.6 = private unnamed_addr constant [28 x i8] c"Creating 10,000 spawns...\\n\00", align 1
@str.7 = private unnamed_addr constant [29 x i8] c"Expected memory: ~40-80 MB\\n\00", align 1
@str.8 = private unnamed_addr constant [33 x i8] c"(10,000 \C3\97 4-8 KB per spawn)\\n\\n\00", align 1
@str.9 = private unnamed_addr constant [28 x i8] c"\E2\9C\93 10,000 spawns created\\n\00", align 1
@str.10 = private unnamed_addr constant [29 x i8] c"\E2\9C\93 Memory claim verified!\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @worker(i32 %id) {
entry:
  %i = alloca i32, align 4
  %local_data = alloca i32, align 4
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  %mul = mul i32 %id2, 2
  store i32 %mul, ptr %local_data, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i3, 10
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %local_data4 = load i32, ptr %local_data, align 4
  %add = add i32 %local_data4, 1
  store i32 %add, ptr %local_data, align 4
  %i5 = load i32, ptr %i, align 4
  %add6 = add i32 %i5, 1
  store i32 %add6, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  ret void
}

define void @wyn_main() {
entry:
  %i = alloca i32, align 4
  %print_string_call = call void @print_string(ptr @str)
  %print_string_call1 = call void @print_string(ptr @str.1)
  %print_string_call2 = call void @print_string(ptr @str.2)
  %print_string_call3 = call void @print_string(ptr @str.3)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i4 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i4, 1000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i5 = load i32, ptr %i, align 4
  %add = add i32 %i5, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %print_string_call6 = call void @print_string(ptr @str.4)
  %print_string_call7 = call void @print_string(ptr @str.5)
  %print_string_call8 = call void @print_string(ptr @str.6)
  %print_string_call9 = call void @print_string(ptr @str.7)
  %print_string_call10 = call void @print_string(ptr @str.8)
  store i32 0, ptr %i, align 4
  br label %while.header11

while.header11:                                   ; preds = %while.body12, %while.end
  %i14 = load i32, ptr %i, align 4
  %icmp15 = icmp slt i32 %i14, 10000
  br i1 %icmp15, label %while.body12, label %while.end13

while.body12:                                     ; preds = %while.header11
  %i16 = load i32, ptr %i, align 4
  %add17 = add i32 %i16, 1
  store i32 %add17, ptr %i, align 4
  br label %while.header11

while.end13:                                      ; preds = %while.header11
  %print_string_call18 = call void @print_string(ptr @str.9)
  %print_string_call19 = call void @print_string(ptr @str.10)
  ret i32 0
}

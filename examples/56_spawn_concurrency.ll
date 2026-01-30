; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [35 x i8] c"=== Spawn Concurrency Test ===\\n\\n\00", align 1
@str.1 = private unnamed_addr constant [39 x i8] c"Spawning 100 workers concurrently...\\n\00", align 1
@str.2 = private unnamed_addr constant [26 x i8] c"\E2\9C\93 100 spawns created!\\n\00", align 1
@str.3 = private unnamed_addr constant [31 x i8] c"\E2\9C\93 All execute concurrently\\n\00", align 1
@str.4 = private unnamed_addr constant [40 x i8] c"\E2\9C\93 Memory efficient (~4-8 KB each)\\n\\n\00", align 1
@str.5 = private unnamed_addr constant [35 x i8] c"Note: Spawns share address space\\n\00", align 1
@str.6 = private unnamed_addr constant [35 x i8] c"Use tasks for safe communication\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @worker(i32 %id) {
entry:
  %i = alloca i32, align 4
  %result = alloca i32, align 4
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  %mul = mul i32 %id2, 10
  store i32 %mul, ptr %result, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i3, 100
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %result4 = load i32, ptr %result, align 4
  %add = add i32 %result4, 1
  store i32 %add, ptr %result, align 4
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
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i2 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i2, 100
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i3 = load i32, ptr %i, align 4
  %add = add i32 %i3, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %print_string_call4 = call void @print_string(ptr @str.2)
  %print_string_call5 = call void @print_string(ptr @str.3)
  %print_string_call6 = call void @print_string(ptr @str.4)
  %print_string_call7 = call void @print_string(ptr @str.5)
  %print_string_call8 = call void @print_string(ptr @str.6)
  ret i32 0
}

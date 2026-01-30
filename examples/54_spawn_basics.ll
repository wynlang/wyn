; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [37 x i8] c"Spawning 1000 lightweight tasks...\\n\00", align 1
@str.1 = private unnamed_addr constant [22 x i8] c"All spawns created!\\n\00", align 1
@str.2 = private unnamed_addr constant [38 x i8] c"Memory efficient: ~4-8 KB per spawn\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @worker(i32 %id) {
entry:
  %x = alloca i32, align 4
  %id1 = alloca i32, align 4
  store i32 %id, ptr %id1, align 4
  %id2 = load i32, ptr %id1, align 4
  %mul = mul i32 %id2, 2
  store i32 %mul, ptr %x, align 4
  ret void
}

define void @wyn_main() {
entry:
  %i = alloca i32, align 4
  %print_string_call = call void @print_string(ptr @str)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i1 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i1, 1000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i2 = load i32, ptr %i, align 4
  %add = add i32 %i2, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %print_string_call3 = call void @print_string(ptr @str.1)
  %print_string_call4 = call void @print_string(ptr @str.2)
  ret i32 0
}

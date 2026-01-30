; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [42 x i8] c"Spawning 1,000,000 lightweight tasks...\\n\00", align 1
@str.1 = private unnamed_addr constant [42 x i8] c"This would require 8TB with OS threads!\\n\00", align 1
@str.2 = private unnamed_addr constant [30 x i8] c"With spawns: only ~4-8 GB\\n\\n\00", align 1
@str.3 = private unnamed_addr constant [9 x i8] c"Spawned \00", align 1
@str.4 = private unnamed_addr constant [9 x i8] c" tasks\\n\00", align 1
@str.5 = private unnamed_addr constant [34 x i8] c"\\nAll 1 million spawns created!\\n\00", align 1
@str.6 = private unnamed_addr constant [49 x i8] c"Work-stealing scheduler handles load balancing\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define void @worker() {
entry:
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  ret void
}

define void @wyn_main() {
entry:
  %i = alloca i32, align 4
  %print_string_call = call void @print_string(ptr @str)
  %print_string_call1 = call void @print_string(ptr @str.1)
  %print_string_call2 = call void @print_string(ptr @str.2)
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %if.end, %entry
  %i3 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i3, 1000000
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %i4 = load i32, ptr %i, align 4
  %add = add i32 %i4, 1
  store i32 %add, ptr %i, align 4
  %i5 = load i32, ptr %i, align 4
  %rem = srem i32 %i5, 100000
  %icmp6 = icmp eq i32 %rem, 0
  br i1 %icmp6, label %if.then, label %if.end

while.end:                                        ; preds = %while.header
  %print_string_call10 = call void @print_string(ptr @str.5)
  %print_string_call11 = call void @print_string(ptr @str.6)
  ret i32 0

if.then:                                          ; preds = %while.body
  %print_string_call7 = call void @print_string(ptr @str.3)
  %i8 = load i32, ptr %i, align 4
  %print_call = call void @print(i32 %i8)
  %print_string_call9 = call void @print_string(ptr @str.4)
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  br label %while.header
}

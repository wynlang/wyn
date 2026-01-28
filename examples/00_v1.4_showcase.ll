; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [4 x i8] c"Wyn\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"1.4.0\00", align 1
@str.2 = private unnamed_addr constant [16 x i8] c"  hello world  \00", align 1
@str.3 = private unnamed_addr constant [22 x i8] c"/tmp/wyn_showcase.txt\00", align 1
@str.4 = private unnamed_addr constant [23 x i8] c"Wyn v1.4.0 is amazing!\00", align 1
@str.5 = private unnamed_addr constant [4 x i8] c"Wyn\00", align 1
@str.6 = private unnamed_addr constant [3 x i8] c"is\00", align 1
@str.7 = private unnamed_addr constant [5 x i8] c"fast\00", align 1
@str.8 = private unnamed_addr constant [4 x i8] c"and\00", align 1
@str.9 = private unnamed_addr constant [8 x i8] c"elegant\00", align 1
@str.10 = private unnamed_addr constant [21 x i8] c"  USER@EXAMPLE.COM  \00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %cleaned = alloca i32, align 4
  %email = alloca i32, align 4
  %sentence = alloca i32, align 4
  %parts = alloca i32, align 4
  %score = alloca i32, align 4
  %timestamp = alloca i32, align 4
  %cwd = alloca i32, align 4
  %content = alloca i32, align 4
  %test_file = alloca i32, align 4
  %avg_val = alloca i32, align 4
  %sum_val = alloca i32, align 4
  %max_val = alloca i32, align 4
  %min_val = alloca i32, align 4
  %numbers = alloca i32, align 4
  %processed = alloca i32, align 4
  %text = alloca i32, align 4
  %version = alloca i32, align 4
  %name = alloca i32, align 4
  store ptr @str, ptr %name, align 8
  store ptr @str.1, ptr %version, align 8
  store ptr @str.2, ptr %text, align 8
  %array_literal = alloca [6 x i32], align 4
  %element_ptr = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 0
  store i32 5, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 2
  store i32 8, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 3
  store i32 1, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 4
  store i32 9, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr [6 x i32], ptr %array_literal, i32 0, i32 5
  store i32 3, ptr %element_ptr5, align 4
  store ptr %array_literal, ptr %numbers, align 8
  store ptr @str.3, ptr %test_file, align 8
  store ptr @str.4, ptr %content, align 8
  store i32 95, ptr %score, align 4
  %score6 = load i32, ptr %score, align 4
  %icmp = icmp sge i32 %score6, i32 90
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  br label %if.end

if.else:                                          ; preds = %entry
  %score7 = load i32, ptr %score, align 4
  %icmp8 = icmp sge i32 %score7, i32 80
  br i1 %icmp8, label %if.then9, label %if.else10

if.end:                                           ; preds = %if.end11, %if.then
  %array_literal12 = alloca [5 x i32], align 4
  %element_ptr13 = getelementptr [5 x i32], ptr %array_literal12, i32 0, i32 0
  store ptr @str.5, ptr %element_ptr13, align 8
  %element_ptr14 = getelementptr [5 x i32], ptr %array_literal12, i32 0, i32 1
  store ptr @str.6, ptr %element_ptr14, align 8
  %element_ptr15 = getelementptr [5 x i32], ptr %array_literal12, i32 0, i32 2
  store ptr @str.7, ptr %element_ptr15, align 8
  %element_ptr16 = getelementptr [5 x i32], ptr %array_literal12, i32 0, i32 3
  store ptr @str.8, ptr %element_ptr16, align 8
  %element_ptr17 = getelementptr [5 x i32], ptr %array_literal12, i32 0, i32 4
  store ptr @str.9, ptr %element_ptr17, align 8
  store ptr %array_literal12, ptr %parts, align 8
  store ptr @str.10, ptr %email, align 8
  ret i32 0

if.then9:                                         ; preds = %if.else
  br label %if.end11

if.else10:                                        ; preds = %if.else
  br label %if.end11

if.end11:                                         ; preds = %if.else10, %if.then9
  br label %if.end
}

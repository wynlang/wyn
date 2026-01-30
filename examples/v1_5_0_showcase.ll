; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [36 x i8] c"=== Wyn v1.5.0 Feature Showcase ===\00", align 1
@str.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.2 = private unnamed_addr constant [20 x i8] c"1. Enums with Data:\00", align 1
@str.3 = private unnamed_addr constant [13 x i8] c"   Success: \00", align 1
@str.4 = private unnamed_addr constant [13 x i8] c"   Failure: \00", align 1
@str.5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.6 = private unnamed_addr constant [17 x i8] c"2. Simple Enums:\00", align 1
@str.7 = private unnamed_addr constant [12 x i8] c"   Status: \00", align 1
@str.8 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.9 = private unnamed_addr constant [17 x i8] c"3. HashMap<K,V>:\00", align 1
@str.10 = private unnamed_addr constant [10 x i8] c"   key1: \00", align 1
@str.11 = private unnamed_addr constant [10 x i8] c"   key2: \00", align 1
@str.12 = private unnamed_addr constant [10 x i8] c"   size: \00", align 1
@str.13 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.14 = private unnamed_addr constant [19 x i8] c"4. String Methods:\00", align 1
@str.15 = private unnamed_addr constant [17 x i8] c"hello,world,test\00", align 1
@str.16 = private unnamed_addr constant [18 x i8] c"   Split result: \00", align 1
@str.17 = private unnamed_addr constant [7 x i8] c" parts\00", align 1
@str.18 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.19 = private unnamed_addr constant [18 x i8] c"5. Array Methods:\00", align 1
@str.20 = private unnamed_addr constant [25 x i8] c"   Original: [1,2,3,4,5]\00", align 1
@str.21 = private unnamed_addr constant [20 x i8] c"   Doubled length: \00", align 1
@str.22 = private unnamed_addr constant [9 x i8] c"   Sum: \00", align 1
@str.23 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.24 = private unnamed_addr constant [18 x i8] c"6. Variable Keys:\00", align 1
@str.25 = private unnamed_addr constant [8 x i8] c"dynamic\00", align 1
@str.26 = private unnamed_addr constant [23 x i8] c"   Dynamic key value: \00", align 1
@str.27 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.28 = private unnamed_addr constant [17 x i8] c"7. Bool Methods:\00", align 1
@str.29 = private unnamed_addr constant [14 x i8] c"   Has key1: \00", align 1
@str.30 = private unnamed_addr constant [12 x i8] c"   As int: \00", align 1
@str.31 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.32 = private unnamed_addr constant [30 x i8] c"=== All Features Working! ===\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @create_map() {
entry:
  %m = alloca i32, align 4
  %m1 = load i32, ptr %m, align 4
  ret i32 %m1
}

define i32 @times_two(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %mul = mul i32 %x2, 2
  ret i32 %mul
}

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

define i32 @wyn_main() {
entry:
  %as_int = alloca i32, align 4
  %has_key = alloca i32, align 4
  %dynamic_val = alloca i32, align 4
  %key_name = alloca ptr, align 8
  %sum = alloca i32, align 4
  %doubled = alloca i32, align 4
  %nums = alloca ptr, align 8
  %parts = alloca i32, align 4
  %text = alloca ptr, align 8
  %val2 = alloca i32, align 4
  %val1 = alloca i32, align 4
  %map = alloca i32, align 4
  %status = alloca i32, align 4
  %failure = alloca i32, align 4
  %success = alloca i32, align 4
  %print_string_call = call void @print_string(ptr @str)
  %print_string_call1 = call void @print_string(ptr @str.1)
  %print_string_call2 = call void @print_string(ptr @str.2)
  %print_string_call3 = call void @print_string(ptr @str.5)
  %print_string_call4 = call void @print_string(ptr @str.6)
  %print_string_call5 = call void @print_string(ptr @str.8)
  %print_string_call6 = call void @print_string(ptr @str.9)
  %print_string_call7 = call void @print_string(ptr @str.13)
  %print_string_call8 = call void @print_string(ptr @str.14)
  store ptr @str.15, ptr %text, align 8
  %print_string_call9 = call void @print_string(ptr @str.18)
  %print_string_call10 = call void @print_string(ptr @str.19)
  %array_literal = alloca [5 x i32], align 4
  %element_ptr = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr11 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr11, align 4
  %element_ptr12 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr12, align 4
  %element_ptr13 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 3
  store i32 4, ptr %element_ptr13, align 4
  %element_ptr14 = getelementptr [5 x i32], ptr %array_literal, i32 0, i32 4
  store i32 5, ptr %element_ptr14, align 4
  store ptr %array_literal, ptr %nums, align 8
  %print_string_call15 = call void @print_string(ptr @str.20)
  %print_string_call16 = call void @print_string(ptr @str.23)
  %print_string_call17 = call void @print_string(ptr @str.24)
  store ptr @str.25, ptr %key_name, align 8
  %print_string_call18 = call void @print_string(ptr @str.27)
  %print_string_call19 = call void @print_string(ptr @str.28)
  %print_string_call20 = call void @print_string(ptr @str.31)
  %print_string_call21 = call void @print_string(ptr @str.32)
  ret i32 0
}

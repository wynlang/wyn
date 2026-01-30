; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.2 = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@str.4 = private unnamed_addr constant [10 x i8] c"  HELLO  \00", align 1
@str.5 = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %chained = alloca i32, align 4
  %res = alloca ptr, align 8
  %opt = alloca ptr, align 8
  %float_num = alloca double, align 8
  %int_num = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %text = alloca ptr, align 8
  %empty_arr = alloca ptr, align 8
  %empty_str = alloca ptr, align 8
  %arr = alloca ptr, align 8
  %str = alloca ptr, align 8
  %exit_code = alloca i32, align 4
  store i32 0, ptr %exit_code, align 4
  store ptr @str, ptr %str, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %str3 = load ptr, ptr %str, align 8
  %strlen = call i64 @strlen(ptr %str3)
  %icmp = icmp ne i64 %strlen, i32 5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %exit_code4 = load i32, ptr %exit_code, align 4
  %add = add i32 %exit_code4, 1
  store i32 %add, ptr %exit_code, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %arr5 = load ptr, ptr %arr, align 8
  %strlen6 = call i64 @strlen(ptr %arr5)
  %icmp7 = icmp ne i64 %strlen6, i32 3
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end
  %exit_code10 = load i32, ptr %exit_code, align 4
  %add11 = add i32 %exit_code10, 2
  store i32 %add11, ptr %exit_code, align 4
  br label %if.end9

if.end9:                                          ; preds = %if.then8, %if.end
  store ptr @str.1, ptr %empty_str, align 8
  %array_literal12 = alloca [0 x i32], align 4
  store ptr %array_literal12, ptr %empty_arr, align 8
  %empty_str13 = load ptr, ptr %empty_str, align 8
  %empty_arr14 = load ptr, ptr %empty_arr, align 8
  store ptr @str.2, ptr %text, align 8
  %array_literal15 = alloca [3 x i32], align 4
  %element_ptr16 = getelementptr [3 x i32], ptr %array_literal15, i32 0, i32 0
  store i32 1, ptr %element_ptr16, align 4
  %element_ptr17 = getelementptr [3 x i32], ptr %array_literal15, i32 0, i32 1
  store i32 2, ptr %element_ptr17, align 4
  %element_ptr18 = getelementptr [3 x i32], ptr %array_literal15, i32 0, i32 2
  store i32 3, ptr %element_ptr18, align 4
  store ptr %array_literal15, ptr %numbers, align 8
  %text19 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text19, ptr @str.3)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  %not = xor i32 %contains, -1
  %tobool = icmp ne i32 %not, 0
  br i1 %tobool, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end9
  %exit_code22 = load i32, ptr %exit_code, align 4
  %add23 = add i32 %exit_code22, 16
  store i32 %add23, ptr %exit_code, align 4
  br label %if.end21

if.end21:                                         ; preds = %if.then20, %if.end9
  %numbers24 = load ptr, ptr %numbers, align 8
  %strstr_result25 = call ptr @strstr(ptr %numbers24, i32 2)
  %is_found26 = icmp ne ptr %strstr_result25, null
  %contains27 = zext i1 %is_found26 to i32
  %not28 = xor i32 %contains27, -1
  %tobool29 = icmp ne i32 %not28, 0
  br i1 %tobool29, label %if.then30, label %if.end31

if.then30:                                        ; preds = %if.end21
  %exit_code32 = load i32, ptr %exit_code, align 4
  %add33 = add i32 %exit_code32, 32
  store i32 %add33, ptr %exit_code, align 4
  br label %if.end31

if.end31:                                         ; preds = %if.then30, %if.end21
  store i32 42, ptr %int_num, align 4
  store double 3.140000e+00, ptr %float_num, align 8
  %int_num34 = load i32, ptr %int_num, align 4
  %float_num35 = load double, ptr %float_num, align 8
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %opt, align 8
  %tmp36 = alloca i32, align 4
  store i32 100, ptr %tmp36, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp36, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %res, align 8
  %opt37 = load ptr, ptr %opt, align 8
  %res38 = load ptr, ptr %res, align 8
  %chained39 = load i32, ptr %chained, align 4
  %icmp40 = icmp ne i32 %chained39, ptr @str.5
  br i1 %icmp40, label %if.then41, label %if.end42

if.then41:                                        ; preds = %if.end31
  %exit_code43 = load i32, ptr %exit_code, align 4
  %add44 = add i32 %exit_code43, 1024
  store i32 %add44, ptr %exit_code, align 4
  br label %if.end42

if.end42:                                         ; preds = %if.then41, %if.end31
  %exit_code45 = load i32, ptr %exit_code, align 4
  ret i32 %exit_code45
}

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_ok(ptr, i64)

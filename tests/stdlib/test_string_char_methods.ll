; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [9 x i8] c"hello123\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"12345\00", align 1
@str.3 = private unnamed_addr constant [5 x i8] c"123a\00", align 1
@str.4 = private unnamed_addr constant [9 x i8] c"hello123\00", align 1
@str.5 = private unnamed_addr constant [10 x i8] c"hello 123\00", align 1
@str.6 = private unnamed_addr constant [4 x i8] c"   \00", align 1
@str.7 = private unnamed_addr constant [4 x i8] c" a \00", align 1
@str.8 = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %ch = alloca i32, align 4
  %str = alloca ptr, align 8
  %not_ws = alloca ptr, align 8
  %ws = alloca ptr, align 8
  %not_alnum = alloca ptr, align 8
  %alnum = alloca ptr, align 8
  %not_digit = alloca ptr, align 8
  %digit = alloca ptr, align 8
  %not_alpha = alloca ptr, align 8
  %alpha = alloca ptr, align 8
  store ptr @str, ptr %alpha, align 8
  store ptr @str.1, ptr %not_alpha, align 8
  %alpha1 = load ptr, ptr %alpha, align 8
  %not_alpha2 = load ptr, ptr %not_alpha, align 8
  store ptr @str.2, ptr %digit, align 8
  store ptr @str.3, ptr %not_digit, align 8
  %digit3 = load ptr, ptr %digit, align 8
  %not_digit4 = load ptr, ptr %not_digit, align 8
  store ptr @str.4, ptr %alnum, align 8
  store ptr @str.5, ptr %not_alnum, align 8
  %alnum5 = load ptr, ptr %alnum, align 8
  %not_alnum6 = load ptr, ptr %not_alnum, align 8
  store ptr @str.6, ptr %ws, align 8
  store ptr @str.7, ptr %not_ws, align 8
  %ws7 = load ptr, ptr %ws, align 8
  %not_ws8 = load ptr, ptr %not_ws, align 8
  store ptr @str.8, ptr %str, align 8
  %str9 = load ptr, ptr %str, align 8
  %ch10 = load i32, ptr %ch, align 4
  %strlen = call i64 @strlen(i32 %ch10)
  %icmp = icmp ne i64 %strlen, i32 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 9

if.end:                                           ; preds = %entry
  ret i32 0
}

declare i64 @strlen(ptr)

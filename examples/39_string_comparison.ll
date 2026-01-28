; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.4 = private unnamed_addr constant [7 x i8] c"banana\00", align 1
@str.5 = private unnamed_addr constant [6 x i8] c"zebra\00", align 1
@str.6 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.7 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.8 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.9 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.10 = private unnamed_addr constant [7 x i8] c"banana\00", align 1
@str.11 = private unnamed_addr constant [6 x i8] c"zebra\00", align 1
@str.12 = private unnamed_addr constant [6 x i8] c"zebra\00", align 1
@str.13 = private unnamed_addr constant [6 x i8] c"zebra\00", align 1
@str.14 = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.15 = private unnamed_addr constant [10 x i8] c"--verbose\00", align 1
@str.16 = private unnamed_addr constant [10 x i8] c"--verbose\00", align 1
@str.17 = private unnamed_addr constant [8 x i8] c"--quiet\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %flag = alloca i32, align 4
  %str3 = alloca i32, align 4
  %str2 = alloca i32, align 4
  %str1 = alloca i32, align 4
  store ptr @str, ptr %str1, align 8
  store ptr @str.1, ptr %str2, align 8
  store ptr @str.2, ptr %str3, align 8
  %icmp = icmp slt ptr @str.3, @str.4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %icmp1 = icmp sgt ptr @str.5, @str.6
  br i1 %icmp1, label %if.then2, label %if.end3

if.then2:                                         ; preds = %if.end
  br label %if.end3

if.end3:                                          ; preds = %if.then2, %if.end
  %icmp4 = icmp sle ptr @str.7, @str.8
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end3
  br label %if.end6

if.end6:                                          ; preds = %if.then5, %if.end3
  %icmp7 = icmp sle ptr @str.9, @str.10
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end6
  br label %if.end9

if.end9:                                          ; preds = %if.then8, %if.end6
  %icmp10 = icmp sge ptr @str.11, @str.12
  br i1 %icmp10, label %if.then11, label %if.end12

if.then11:                                        ; preds = %if.end9
  br label %if.end12

if.end12:                                         ; preds = %if.then11, %if.end9
  %icmp13 = icmp sge ptr @str.13, @str.14
  br i1 %icmp13, label %if.then14, label %if.end15

if.then14:                                        ; preds = %if.end12
  br label %if.end15

if.end15:                                         ; preds = %if.then14, %if.end12
  store ptr @str.15, ptr %flag, align 8
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@fmt = private unnamed_addr constant [3 x i8] c"%d\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %s = alloca ptr, align 8
  %mx = alloca i32, align 4
  %m = alloca i32, align 4
  %a = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 -42, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %is_neg = icmp slt i32 %x1, 0
  %neg = sub i32 0, %x1
  %abs = select i1 %is_neg, i32 %neg, i32 %x1
  store i32 %abs, ptr %a, align 4
  %a2 = load i32, ptr %a, align 4
  %icmp = icmp ne i32 %a2, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %x3 = load i32, ptr %x, align 4
  %cmp = icmp slt i32 %x3, -10
  %min = select i1 %cmp, i32 %x3, i32 -10
  store i32 %min, ptr %m, align 4
  %m4 = load i32, ptr %m, align 4
  %icmp5 = icmp ne i32 %m4, -42
  br i1 %icmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  ret i32 2

if.end7:                                          ; preds = %if.end
  %x8 = load i32, ptr %x, align 4
  %cmp9 = icmp sgt i32 %x8, -10
  %max = select i1 %cmp9, i32 %x8, i32 -10
  store i32 %max, ptr %mx, align 4
  %mx10 = load i32, ptr %mx, align 4
  %icmp11 = icmp ne i32 %mx10, -10
  br i1 %icmp11, label %if.then12, label %if.end13

if.then12:                                        ; preds = %if.end7
  ret i32 3

if.end13:                                         ; preds = %if.end7
  %x14 = load i32, ptr %x, align 4
  %buffer = call ptr @malloc(i64 32)
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %buffer, ptr @fmt, i32 %x14)
  store ptr %buffer, ptr %s, align 8
  ret i32 0
}

declare i32 @sprintf(ptr, ptr, ...)

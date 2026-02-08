; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [12 x i8] c"true works!\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [22 x i8] c"This should not print\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [13 x i8] c"false works!\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %f = alloca i1, align 1
  %t = alloca i1, align 1
  store i1 true, ptr %t, align 1
  store i1 false, ptr %f, align 1
  %t1 = load i1, ptr %t, align 1
  br i1 %t1, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %f2 = load i1, ptr %f, align 1
  br i1 %f2, label %if.then3, label %if.else

if.then3:                                         ; preds = %if.end
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  br label %if.end4

if.else:                                          ; preds = %if.end
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  br label %if.end4

if.end4:                                          ; preds = %if.else, %if.then3
  ret i32 0
}

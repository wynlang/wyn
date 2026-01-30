; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [124 x i8] c"This is a reasonably long string to test if length calculation works properly without truncation issues in the LLVM codegen\00", align 1
@str.1 = private unnamed_addr constant [8 x i8] c"Length:\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [27 x i8] c"\E2\9C\93 len() works correctly!\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [31 x i8] c"\E2\9C\97 len() returned wrong value\00", align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %length = alloca i64, align 8
  %text = alloca ptr, align 8
  store ptr @str, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text1)
  store i64 %strlen, ptr %length, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.1)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %length2 = load i64, ptr %length, align 4
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, i64 %length2)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %length3 = load i64, ptr %length, align 4
  %icmp = icmp eq i64 %length3, i32 123
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  ret i32 0

if.else:                                          ; preds = %entry
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  ret i32 1

if.end:                                           ; No predecessors!
  ret i32 0
}

declare i64 @strlen(ptr)

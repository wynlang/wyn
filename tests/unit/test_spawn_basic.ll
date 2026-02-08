; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [15 x i8] c"Worker running\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [17 x i8] c"Testing spawn...\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [20 x i8] c"\E2\9C\93 spawn compiles!\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @worker() {
entry:
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  ret i32 42
}

define i32 @wyn_main() {
entry:
  %0 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %1 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %worker = call i32 @worker()
  %2 = call i32 @usleep(i32 100000)
  %3 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %4 = call i32 (ptr, ...) @printf(ptr @nl.6)
  ret i32 0
}

declare i32 @usleep(i32)

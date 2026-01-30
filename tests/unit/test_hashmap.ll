; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [31 x i8] c"HashMap literal syntax works\\n\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %hm = alloca i32, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  ret i32 0
}

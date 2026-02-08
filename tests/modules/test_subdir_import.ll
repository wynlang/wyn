; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca ptr, align 8
  %uppercase = call ptr @uppercase(ptr @str)
  store ptr %uppercase, ptr %result, align 8
  ret i32 0
}

define ptr @uppercase(ptr %s) {
entry:
  %s1 = alloca ptr, align 8
  store ptr %s, ptr %s1, align 8
  %s2 = load ptr, ptr %s1, align 8
  ret ptr %s2
}

define ptr @lowercase(ptr %s) {
entry:
  %s1 = alloca ptr, align 8
  store ptr %s, ptr %s1, align 8
  %s2 = load ptr, ptr %s1, align 8
  ret ptr %s2
}

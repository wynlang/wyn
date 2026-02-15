; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [1 x i8] zeroinitializer, align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define void @wyn_main() {
entry:
  %padded_right = alloca i32, align 4
  %padded_left = alloca i32, align 4
  %repeated = alloca i32, align 4
  %trimmed = alloca i32, align 4
  %idx2 = alloca i32, align 4
  %idx1 = alloca i32, align 4
  %ends = alloca i32, align 4
  %starts = alloca i32, align 4
  %contains2 = alloca i32, align 4
  %contains1 = alloca i32, align 4
  %sub = alloca i32, align 4
  %replaced = alloca i32, align 4
  %parts = alloca i32, align 4
  %count = alloca i32, align 4
  %empty = alloca i32, align 4
  store ptr @str, ptr %empty, align 8
  store i32 0, ptr %count, align 4
  ret void
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [33 x i8] c"test_output/level1/level2/level3\00", align 1
@str.1 = private unnamed_addr constant [28 x i8] c"test_output/level1/test.txt\00", align 1
@str.2 = private unnamed_addr constant [40 x i8] c"Hello from Wyn!\\nThis is a test file.\\n\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %removed = alloca i32, align 4
  %mtime = alloca i32, align 4
  %content = alloca i32, align 4
  %test_file = alloca i32, align 4
  %created = alloca i32, align 4
  %test_dir = alloca i32, align 4
  store ptr @str, ptr %test_dir, align 8
  store ptr @str.1, ptr %test_file, align 8
  store ptr @str.2, ptr %content, align 8
  ret i32 0
}

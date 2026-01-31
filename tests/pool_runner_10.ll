; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [76 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_01_variables.out\00", align 1
@str.1 = private unnamed_addr constant [76 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_02_functions.out\00", align 1
@str.2 = private unnamed_addr constant [74 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_03_structs.out\00", align 1
@str.3 = private unnamed_addr constant [72 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_04_enums.out\00", align 1
@str.4 = private unnamed_addr constant [73 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_05_arrays.out\00", align 1
@str.5 = private unnamed_addr constant [80 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_06_result_option.out\00", align 1
@str.6 = private unnamed_addr constant [83 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_07_pattern_matching.out\00", align 1
@str.7 = private unnamed_addr constant [79 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_08_control_flow.out\00", align 1
@str.8 = private unnamed_addr constant [79 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_09_type_aliases.out\00", align 1
@str.9 = private unnamed_addr constant [75 x i8] c"timeout 2 /Users/aoaws/src/ao/wyn-lang/wyn/tests/unit/test_10_closures.out\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %failures = alloca i32, align 4
  %pool_add = call i32 @pool_add_task(ptr @str)
  %pool_add1 = call i32 @pool_add_task(ptr @str.1)
  %pool_add2 = call i32 @pool_add_task(ptr @str.2)
  %pool_add3 = call i32 @pool_add_task(ptr @str.3)
  %pool_add4 = call i32 @pool_add_task(ptr @str.4)
  %pool_add5 = call i32 @pool_add_task(ptr @str.5)
  %pool_add6 = call i32 @pool_add_task(ptr @str.6)
  %pool_add7 = call i32 @pool_add_task(ptr @str.7)
  %pool_add8 = call i32 @pool_add_task(ptr @str.8)
  %pool_add9 = call i32 @pool_add_task(ptr @str.9)
  %pool_start = call void @pool_start(i32 10)
  %pool_wait = call i32 @pool_wait()
  store i32 %pool_wait, ptr %failures, align 4
  %failures10 = load i32, ptr %failures, align 4
  ret i32 %failures10
}

declare i32 @pool_add_task(ptr)

declare void @pool_start(i32)

declare i32 @pool_wait()

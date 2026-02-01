; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [109 x i8] c"./wyn tests/unit/test_01_variables.wyn >/dev/null 2>&1 && ./tests/unit/test_01_variables.out >/dev/null 2>&1\00", align 1
@str.1 = private unnamed_addr constant [109 x i8] c"./wyn tests/unit/test_02_functions.wyn >/dev/null 2>&1 && ./tests/unit/test_02_functions.out >/dev/null 2>&1\00", align 1
@str.2 = private unnamed_addr constant [105 x i8] c"./wyn tests/unit/test_03_structs.wyn >/dev/null 2>&1 && ./tests/unit/test_03_structs.out >/dev/null 2>&1\00", align 1
@str.3 = private unnamed_addr constant [101 x i8] c"./wyn tests/unit/test_04_enums.wyn >/dev/null 2>&1 && ./tests/unit/test_04_enums.out >/dev/null 2>&1\00", align 1
@str.4 = private unnamed_addr constant [103 x i8] c"./wyn tests/unit/test_05_arrays.wyn >/dev/null 2>&1 && ./tests/unit/test_05_arrays.out >/dev/null 2>&1\00", align 1
@str.5 = private unnamed_addr constant [117 x i8] c"./wyn tests/unit/test_06_result_option.wyn >/dev/null 2>&1 && ./tests/unit/test_06_result_option.out >/dev/null 2>&1\00", align 1
@str.6 = private unnamed_addr constant [123 x i8] c"./wyn tests/unit/test_07_pattern_matching.wyn >/dev/null 2>&1 && ./tests/unit/test_07_pattern_matching.out >/dev/null 2>&1\00", align 1
@str.7 = private unnamed_addr constant [115 x i8] c"./wyn tests/unit/test_08_control_flow.wyn >/dev/null 2>&1 && ./tests/unit/test_08_control_flow.out >/dev/null 2>&1\00", align 1
@str.8 = private unnamed_addr constant [115 x i8] c"./wyn tests/unit/test_09_type_aliases.wyn >/dev/null 2>&1 && ./tests/unit/test_09_type_aliases.out >/dev/null 2>&1\00", align 1
@str.9 = private unnamed_addr constant [107 x i8] c"./wyn tests/unit/test_10_generics.wyn >/dev/null 2>&1 && ./tests/unit/test_10_generics.out >/dev/null 2>&1\00", align 1
@str.10 = private unnamed_addr constant [22 x i8] c"\E2\9C\93 Test run complete\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_01() {
entry:
  %system = call i32 @system(ptr @str)
  ret i32 0
}

define i32 @test_02() {
entry:
  %system = call i32 @system(ptr @str.1)
  ret i32 0
}

define i32 @test_03() {
entry:
  %system = call i32 @system(ptr @str.2)
  ret i32 0
}

define i32 @test_04() {
entry:
  %system = call i32 @system(ptr @str.3)
  ret i32 0
}

define i32 @test_05() {
entry:
  %system = call i32 @system(ptr @str.4)
  ret i32 0
}

define i32 @test_06() {
entry:
  %system = call i32 @system(ptr @str.5)
  ret i32 0
}

define i32 @test_07() {
entry:
  %system = call i32 @system(ptr @str.6)
  ret i32 0
}

define i32 @test_08() {
entry:
  %system = call i32 @system(ptr @str.7)
  ret i32 0
}

define i32 @test_09() {
entry:
  %system = call i32 @system(ptr @str.8)
  ret i32 0
}

define i32 @test_10() {
entry:
  %system = call i32 @system(ptr @str.9)
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %test_01 = call i32 @test_01()
  %test_02 = call i32 @test_02()
  %test_03 = call i32 @test_03()
  %test_04 = call i32 @test_04()
  %test_05 = call i32 @test_05()
  %test_06 = call i32 @test_06()
  %test_07 = call i32 @test_07()
  %test_08 = call i32 @test_08()
  %test_09 = call i32 @test_09()
  %test_10 = call i32 @test_10()
  %0 = call i32 @usleep(i32 3000000)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str.10)
  %2 = call i32 (ptr, ...) @printf(ptr @nl)
  ret i32 0
}

declare i32 @system(ptr)

declare i32 @usleep(i32)

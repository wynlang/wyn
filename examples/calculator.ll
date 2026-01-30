; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @calculate(i32 %op, i32 %a, i32 %b) {
entry:
  %b3 = alloca i32, align 4
  %a2 = alloca i32, align 4
  %op1 = alloca i32, align 4
  store i32 %op, ptr %op1, align 4
  store i32 %a, ptr %a2, align 4
  store i32 %b, ptr %b3, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %result3 = alloca i32, align 4
  %result2 = alloca i32, align 4
  %result1 = alloca i32, align 4
  %result11 = load i32, ptr %result1, align 4
  %result22 = load i32, ptr %result2, align 4
  %add = add i32 %result11, %result22
  %result33 = load i32, ptr %result3, align 4
  %add4 = add i32 %add, %result33
  ret i32 %add4
}

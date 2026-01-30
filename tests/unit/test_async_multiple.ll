; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @get_a() {
entry:
  ret i32 10
}

define i32 @get_b() {
entry:
  ret i32 20
}

define i32 @get_c() {
entry:
  ret i32 30
}

define i32 @wyn_main() {
entry:
  %c = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %add = add i32 %a1, %b2
  %c3 = load i32, ptr %c, align 4
  %add4 = add i32 %add, %c3
  ret i32 %add4
}

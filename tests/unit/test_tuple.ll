; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %s = alloca i32, align 4
  %r = alloca i32, align 4
  %q = alloca i32, align 4
  %p = alloca i32, align 4
  %quad = alloca i32, align 4
  %c = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %triple = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %pair = alloca i32, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %add = add i32 %x1, %y2
  %a3 = load i32, ptr %a, align 4
  %add4 = add i32 %add, %a3
  %b5 = load i32, ptr %b, align 4
  %add6 = add i32 %add4, %b5
  %c7 = load i32, ptr %c, align 4
  %add8 = add i32 %add6, %c7
  %p9 = load i32, ptr %p, align 4
  %add10 = add i32 %add8, %p9
  %q11 = load i32, ptr %q, align 4
  %add12 = add i32 %add10, %q11
  %r13 = load i32, ptr %r, align 4
  %add14 = add i32 %add12, %r13
  %s15 = load i32, ptr %s, align 4
  %add16 = add i32 %add14, %s15
  ret i32 %add16
}

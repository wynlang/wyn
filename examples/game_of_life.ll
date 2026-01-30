; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @cell_new(i32 %x, i32 %y, i32 %alive) {
entry:
  %alive3 = alloca i32, align 4
  %y2 = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  store i32 %y, ptr %y2, align 4
  store i32 %alive, ptr %alive3, align 4
  ret i32 0
}

define i32 @cell_is_alive(i32 %c) {
entry:
  %c1 = alloca i32, align 4
  store i32 %c, ptr %c1, align 4
  ret i32 0
}

define i32 @cell_toggle(i32 %c) {
entry:
  %new_alive = alloca i32, align 4
  %c1 = alloca i32, align 4
  store i32 %c, ptr %c1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %cell3 = alloca i32, align 4
  %cell2 = alloca i32, align 4
  %cell1 = alloca i32, align 4
  %cell_new = call i32 @cell_new(i32 0, i32 0, i32 1)
  store i32 %cell_new, ptr %cell1, align 4
  %cell_new1 = call i32 @cell_new(i32 1, i32 0, i32 0)
  store i32 %cell_new1, ptr %cell2, align 4
  %cell22 = load i32, ptr %cell2, align 4
  %cell_toggle = call i32 @cell_toggle(i32 %cell22)
  store i32 %cell_toggle, ptr %cell3, align 4
  %cell13 = load i32, ptr %cell1, align 4
  %cell_is_alive = call i32 @cell_is_alive(i32 %cell13)
  %cell34 = load i32, ptr %cell3, align 4
  %cell_is_alive5 = call i32 @cell_is_alive(i32 %cell34)
  %add = add i32 %cell_is_alive, %cell_is_alive5
  ret i32 %add
}

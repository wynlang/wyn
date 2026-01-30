; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @rect_area(i32 %r) {
entry:
  %r1 = alloca i32, align 4
  store i32 %r, ptr %r1, align 4
  ret i32 0
}

define i32 @rect_perimeter(i32 %r) {
entry:
  %r1 = alloca i32, align 4
  store i32 %r, ptr %r1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %perim = alloca i32, align 4
  %area = alloca i32, align 4
  %rect = alloca i32, align 4
  %rect1 = load i32, ptr %rect, align 4
  %rect_area = call i32 @rect_area(i32 %rect1)
  store i32 %rect_area, ptr %area, align 4
  %rect2 = load i32, ptr %rect, align 4
  %rect_perimeter = call i32 @rect_perimeter(i32 %rect2)
  store i32 %rect_perimeter, ptr %perim, align 4
  %area3 = load i32, ptr %area, align 4
  %perim4 = load i32, ptr %perim, align 4
  %add = add i32 %area3, %perim4
  ret i32 %add
}

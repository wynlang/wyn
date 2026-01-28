; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @distance_from_origin(i32 %p) {
entry:
  %p1 = alloca i32, align 4
  store i32 %p, ptr %p1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %dist2 = alloca i32, align 4
  %dist1 = alloca i32, align 4
  %p2 = alloca i32, align 4
  %p1 = alloca i32, align 4
  %p11 = load i32, ptr %p1, align 4
  %distance_from_origin = call i32 @distance_from_origin(i32 %p11)
  store i32 %distance_from_origin, ptr %dist1, align 4
  %p22 = load i32, ptr %p2, align 4
  %distance_from_origin3 = call i32 @distance_from_origin(i32 %p22)
  store i32 %distance_from_origin3, ptr %dist2, align 4
  ret i32 0
}

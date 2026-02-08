; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %icmp = icmp sgt i32 %x1, 5
  %x2 = load i32, ptr %x, align 4
  %icmp3 = icmp slt i32 %x2, 15
  %x4 = load i32, ptr %x, align 4
  %icmp5 = icmp sge i32 %x4, 10
  %x6 = load i32, ptr %x, align 4
  %icmp7 = icmp sle i32 %x6, 10
  %x8 = load i32, ptr %x, align 4
  %icmp9 = icmp ne i32 %x8, 5
  ret i32 0
}

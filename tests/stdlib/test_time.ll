; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %after = alloca i32, align 4
  %now = alloca i32, align 4
  %now1 = load i32, ptr %now, align 4
  %icmp = icmp eq i32 %now1, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %after2 = load i32, ptr %after, align 4
  %now3 = load i32, ptr %now, align 4
  %icmp4 = icmp sle i32 %after2, %now3
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  ret i32 0
}

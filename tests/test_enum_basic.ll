; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@RED = internal constant i32 0
@GREEN = internal constant i32 1
@BLUE = internal constant i32 2

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %c = alloca i32, align 4
  %RED = load i32, ptr @RED, align 4
  store i32 %RED, ptr %c, align 4
  %c1 = load i32, ptr %c, align 4
  %RED2 = load i32, ptr @RED, align 4
  %icmp = icmp eq i32 %c1, %RED2
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 0

if.end:                                           ; preds = %entry
  ret i32 1
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %a = alloca i32, align 4
  %z = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %icmp = icmp eq i32 %x1, 5
  br i1 %icmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i32 10, ptr %y, align 4
  br label %if.end

if.else:                                          ; preds = %entry
  store i32 20, ptr %z, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  store i32 42, ptr %a, align 4
  %a2 = load i32, ptr %a, align 4
  ret i32 %a2
}

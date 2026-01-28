; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define void @wyn_main() {
entry:
  %result5 = alloca i32, align 4
  %result4 = alloca i32, align 4
  %result3 = alloca i32, align 4
  %result2 = alloca i32, align 4
  %result1 = alloca i32, align 4
  %result21 = load i32, ptr %result2, align 4
  %tobool = icmp ne i32 %result21, 0
  br i1 %tobool, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  br label %if.end

if.else:                                          ; preds = %entry
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %result32 = load i32, ptr %result3, align 4
  %tobool3 = icmp ne i32 %result32, 0
  br i1 %tobool3, label %if.then4, label %if.else5

if.then4:                                         ; preds = %if.end
  br label %if.end6

if.else5:                                         ; preds = %if.end
  br label %if.end6

if.end6:                                          ; preds = %if.else5, %if.then4
  ret void
}

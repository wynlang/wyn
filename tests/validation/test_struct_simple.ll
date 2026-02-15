; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Rectangle = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %rect = alloca %Rectangle, align 8
  store %Rectangle { i32 100, i32 50 }, ptr %rect, align 4
  %rect1 = load %Rectangle, ptr %rect, align 4
  %field_val = extractvalue %Rectangle %rect1, 0
  %icmp = icmp ne i32 %field_val, 100
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %rect2 = load %Rectangle, ptr %rect, align 4
  %field_val3 = extractvalue %Rectangle %rect2, 1
  %icmp4 = icmp ne i32 %field_val3, 50
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  ret i32 0
}

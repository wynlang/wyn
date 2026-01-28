; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %has_banana = alloca i32, align 4
  %has_grape = alloca i32, align 4
  %set = alloca i32, align 4
  %has_grape1 = load i32, ptr %has_grape, align 4
  %icmp = icmp eq i32 %has_grape1, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %has_banana2 = load i32, ptr %has_banana, align 4
  %icmp3 = icmp eq i32 %has_banana2, i32 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  br label %if.end5

if.end5:                                          ; preds = %if.then4, %if.end
  ret i32 0
}

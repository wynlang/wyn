; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [45 x i8] c"{\\\22name\\\22:\\\22Alice\\\22,\\\22age\\\22:30,\\\22score\\\22:95}\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %score = alloca i32, align 4
  %age = alloca i32, align 4
  %name = alloca i32, align 4
  %json = alloca i32, align 4
  %json_text = alloca i32, align 4
  store ptr @str, ptr %json_text, align 8
  %json1 = load i32, ptr %json, align 4
  %icmp = icmp eq i32 %json1, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %name2 = load i32, ptr %name, align 4
  %icmp3 = icmp ne i32 %name2, i32 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  br label %if.end5

if.end5:                                          ; preds = %if.then4, %if.end
  ret i32 0
}

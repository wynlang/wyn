; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [31 x i8] c"{\\\22name\\\22:\\\22John\\\22,\\\22age\\\22:30}\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %age = alloca i32, align 4
  %name = alloca i32, align 4
  %json = alloca i32, align 4
  %json_text = alloca ptr, align 8
  store ptr @str, ptr %json_text, align 8
  %json1 = load i32, ptr %json, align 4
  %icmp = icmp eq i32 %json1, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %name2 = load i32, ptr %name, align 4
  %icmp3 = icmp eq i32 %name2, 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %age6 = load i32, ptr %age, align 4
  %icmp7 = icmp ne i32 %age6, 30
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end5
  ret i32 3

if.end9:                                          ; preds = %if.end5
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [15 x i8] c"File not found\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %value = alloca i32, align 4
  %err_msg = alloca ptr, align 8
  store ptr @str, ptr %err_msg, align 8
  %err_msg1 = load ptr, ptr %err_msg, align 8
  %strlen = call i64 @strlen(ptr %err_msg1)
  %icmp = icmp eq i64 %strlen, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  store i32 5, ptr %value, align 4
  %value2 = load i32, ptr %value, align 4
  %icmp3 = icmp ne i32 %value2, 5
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  ret i32 0
}

declare i64 @strlen(ptr)

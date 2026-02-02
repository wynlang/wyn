; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %value = alloca i32, align 4
  %result = alloca i32, align 4
  %path = alloca i32, align 4
  %path1 = load i32, ptr %path, align 4
  %strlen = call i64 @strlen(i32 %path1)
  %icmp = icmp eq i64 %strlen, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %result2 = load i32, ptr %result, align 4
  %icmp3 = icmp eq i32 %result2, 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %value6 = load i32, ptr %value, align 4
  %strlen7 = call i64 @strlen(i32 %value6)
  %icmp8 = icmp eq i64 %strlen7, i32 0
  br i1 %icmp8, label %if.then9, label %if.end10

if.then9:                                         ; preds = %if.end5
  ret i32 3

if.end10:                                         ; preds = %if.end5
  ret i32 0
}

declare i64 @strlen(ptr)

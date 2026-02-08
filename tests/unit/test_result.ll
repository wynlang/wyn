; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @divide(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %b3 = load i32, ptr %b2, align 4
  %icmp = icmp eq i32 %b3, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %tmp = alloca i32, align 4
  store i32 1, ptr %tmp, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  ret ptr %wyn_err

if.end:                                           ; preds = %entry
  %a4 = load i32, ptr %a1, align 4
  %b5 = load i32, ptr %b2, align 4
  %div = sdiv i32 %a4, %b5
  %tmp6 = alloca i32, align 4
  store i32 %div, ptr %tmp6, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp6, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  ret ptr %wyn_ok
}

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %divide = call i32 @divide(i32 10, i32 2)
  store i32 %divide, ptr %result, align 4
  ret i32 5
}

declare ptr @wyn_err(ptr, i64)

declare ptr @wyn_ok(ptr, i64)

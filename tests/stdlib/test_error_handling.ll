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
  ret i32 -1

if.end:                                           ; preds = %entry
  %a4 = load i32, ptr %a1, align 4
  %b5 = load i32, ptr %b2, align 4
  %div = sdiv i32 %a4, %b5
  ret i32 %div
}

define i32 @safe_divide(i32 %a, i32 %b) {
entry:
  %result = alloca i32, align 4
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %divide = call i32 @divide(i32 %a3, i32 %b4)
  store i32 %divide, ptr %result, align 4
  %result5 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result5, -1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 0

if.end:                                           ; preds = %entry
  %result6 = load i32, ptr %result, align 4
  ret i32 %result6
}

define i32 @wyn_main() {
entry:
  %result2 = alloca i32, align 4
  %result1 = alloca i32, align 4
  %safe_divide = call i32 @safe_divide(i32 10, i32 2)
  store i32 %safe_divide, ptr %result1, align 4
  %result11 = load i32, ptr %result1, align 4
  %icmp = icmp ne i32 %result11, 5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %safe_divide2 = call i32 @safe_divide(i32 10, i32 0)
  store i32 %safe_divide2, ptr %result2, align 4
  %result23 = load i32, ptr %result2, align 4
  %icmp4 = icmp ne i32 %result23, 0
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  ret i32 0
}

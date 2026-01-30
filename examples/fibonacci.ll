; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @fibonacci(i32 %n) {
entry:
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  %n2 = load i32, ptr %n1, align 4
  %icmp = icmp sle i32 %n2, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %n3 = load i32, ptr %n1, align 4
  ret i32 %n3

if.end:                                           ; preds = %entry
  %n4 = load i32, ptr %n1, align 4
  %sub = sub i32 %n4, 1
  %fibonacci = call i32 @fibonacci(i32 %sub)
  %n5 = load i32, ptr %n1, align 4
  %sub6 = sub i32 %n5, 2
  %fibonacci7 = call i32 @fibonacci(i32 %sub6)
  %add = add i32 %fibonacci, %fibonacci7
  ret i32 %add
}

define i32 @wyn_main() {
entry:
  %fib7 = alloca i32, align 4
  %fib5 = alloca i32, align 4
  %fibonacci = call i32 @fibonacci(i32 5)
  store i32 %fibonacci, ptr %fib5, align 4
  %fibonacci1 = call i32 @fibonacci(i32 7)
  store i32 %fibonacci1, ptr %fib7, align 4
  %fib52 = load i32, ptr %fib5, align 4
  %fib73 = load i32, ptr %fib7, align 4
  %add = add i32 %fib52, %fib73
  ret i32 %add
}

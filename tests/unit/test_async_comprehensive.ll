; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @compute() {
entry:
  ret i32 10
}

define i32 @add(i32 %x, i32 %y) {
entry:
  %y2 = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  store i32 %y, ptr %y2, align 4
  %x3 = load i32, ptr %x1, align 4
  %y4 = load i32, ptr %y2, align 4
  %add = add i32 %x3, %y4
  ret i32 %add
}

define i32 @chain() {
entry:
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %add = add i32 %a1, %b2
  ret i32 %add
}

define i32 @wyn_main() {
entry:
  %result3 = alloca i32, align 4
  %result2 = alloca i32, align 4
  %result1 = alloca i32, align 4
  %result11 = load i32, ptr %result1, align 4
  %icmp = icmp ne i32 %result11, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %result22 = load i32, ptr %result2, align 4
  %icmp3 = icmp ne i32 %result22, 12
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %result36 = load i32, ptr %result3, align 4
  %icmp7 = icmp ne i32 %result36, 20
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end5
  ret i32 3

if.end9:                                          ; preds = %if.end5
  ret i32 99
}

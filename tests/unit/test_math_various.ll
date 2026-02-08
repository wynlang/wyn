; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @abs_test(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %icmp = icmp slt i32 %x2, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %x3 = load i32, ptr %x1, align 4
  %sub = sub i32 0, %x3
  ret i32 %sub

if.end:                                           ; preds = %entry
  %x4 = load i32, ptr %x1, align 4
  ret i32 %x4
}

define i32 @max_test(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %icmp = icmp sgt i32 %a3, %b4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %a5 = load i32, ptr %a1, align 4
  ret i32 %a5

if.end:                                           ; preds = %entry
  %b6 = load i32, ptr %b2, align 4
  ret i32 %b6
}

define i32 @wyn_pow(i32 %x, i32 %n) {
entry:
  %i = alloca i32, align 4
  %result = alloca i32, align 4
  %n2 = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  store i32 %n, ptr %n2, align 4
  store i32 1, ptr %result, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %n4 = load i32, ptr %n2, align 4
  %icmp = icmp slt i32 %i3, %n4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %result5 = load i32, ptr %result, align 4
  %x6 = load i32, ptr %x1, align 4
  %mul = mul i32 %result5, %x6
  store i32 %mul, ptr %result, align 4
  %i7 = load i32, ptr %i, align 4
  %add = add i32 %i7, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %result8 = load i32, ptr %result, align 4
  ret i32 %result8
}

define i32 @wyn_main() {
entry:
  %p2 = alloca i32, align 4
  %p1 = alloca i32, align 4
  %m2 = alloca i32, align 4
  %m1 = alloca i32, align 4
  %a2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  %abs_test = call i32 @abs_test(i32 -15)
  store i32 %abs_test, ptr %a1, align 4
  %abs_test1 = call i32 @abs_test(i32 20)
  store i32 %abs_test1, ptr %a2, align 4
  %max_test = call i32 @max_test(i32 100, i32 50)
  store i32 %max_test, ptr %m1, align 4
  %max_test2 = call i32 @max_test(i32 3, i32 7)
  store i32 %max_test2, ptr %m2, align 4
  %wyn_pow = call i32 @wyn_pow(i32 3, i32 2)
  store i32 %wyn_pow, ptr %p1, align 4
  %wyn_pow3 = call i32 @wyn_pow(i32 2, i32 4)
  store i32 %wyn_pow3, ptr %p2, align 4
  %a14 = load i32, ptr %a1, align 4
  %a25 = load i32, ptr %a2, align 4
  %add = add i32 %a14, %a25
  %m16 = load i32, ptr %m1, align 4
  %add7 = add i32 %add, %m16
  %m28 = load i32, ptr %m2, align 4
  %add9 = add i32 %add7, %m28
  %p110 = load i32, ptr %p1, align 4
  %add11 = add i32 %add9, %p110
  %p212 = load i32, ptr %p2, align 4
  %add13 = add i32 %add11, %p212
  ret i32 %add13
}

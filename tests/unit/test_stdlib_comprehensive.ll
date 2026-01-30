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

define i32 @min_test(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %icmp = icmp slt i32 %a3, %b4
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
  %res = alloca i32, align 4
  %n2 = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  store i32 %n, ptr %n2, align 4
  store i32 1, ptr %res, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %n4 = load i32, ptr %n2, align 4
  %icmp = icmp slt i32 %i3, %n4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %res5 = load i32, ptr %res, align 4
  %x6 = load i32, ptr %x1, align 4
  %mul = mul i32 %res5, %x6
  store i32 %mul, ptr %res, align 4
  %i7 = load i32, ptr %i, align 4
  %add = add i32 %i7, 1
  store i32 %add, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %res8 = load i32, ptr %res, align 4
  ret i32 %res8
}

define i32 @wyn_main() {
entry:
  %p = alloca i32, align 4
  %n = alloca i32, align 4
  %m = alloca i32, align 4
  %a = alloca i32, align 4
  %abs_test = call i32 @abs_test(i32 -10)
  store i32 %abs_test, ptr %a, align 4
  %max_test = call i32 @max_test(i32 5, i32 15)
  store i32 %max_test, ptr %m, align 4
  %min_test = call i32 @min_test(i32 20, i32 10)
  store i32 %min_test, ptr %n, align 4
  %wyn_pow = call i32 @wyn_pow(i32 2, i32 4)
  store i32 %wyn_pow, ptr %p, align 4
  %a1 = load i32, ptr %a, align 4
  %m2 = load i32, ptr %m, align 4
  %add = add i32 %a1, %m2
  %n3 = load i32, ptr %n, align 4
  %add4 = add i32 %add, %n3
  %p5 = load i32, ptr %p, align 4
  %add6 = add i32 %add4, %p5
  ret i32 %add6
}

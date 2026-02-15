; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Point.0 = type { i32, i32 }
%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result2 = alloca i32, align 4
  %p2 = alloca %Point.0, align 8
  %result = alloca i32, align 4
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %match.result = alloca i32, align 4
  %x = extractvalue %Point %p1, 0
  %x5 = alloca i32, align 4
  store i32 %x, ptr %x5, align 4
  %y = extractvalue %Point %p1, 1
  %y6 = alloca i32, align 4
  store i32 %y, ptr %y6, align 4
  %x7 = load i32, ptr %x5, align 4
  %icmp = icmp sgt i32 %x7, 5
  br i1 %icmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm4, %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result16 = load i32, ptr %result, align 4
  %icmp17 = icmp ne i32 %result16, 30
  br i1 %icmp17, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  %x8 = load i32, ptr %x5, align 4
  %y9 = load i32, ptr %y6, align 4
  %add = add i32 %x8, %y9
  store i32 %add, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  %x10 = extractvalue %Point %p1, 0
  %x11 = alloca i32, align 4
  store i32 %x10, ptr %x11, align 4
  %y12 = extractvalue %Point %p1, 1
  %y13 = alloca i32, align 4
  store i32 %y12, ptr %y13, align 4
  %x14 = load i32, ptr %x11, align 4
  %y15 = load i32, ptr %y13, align 4
  %sub = sub i32 %x14, %y15
  store i32 %sub, ptr %match.result, align 4
  br label %match.end

match.next3:                                      ; No predecessors!
  br label %match.arm4

match.arm4:                                       ; preds = %match.next3
  store i32 0, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  store %Point.0 { i32 2, i32 8 }, ptr %p2, align 4
  %p218 = load %Point.0, ptr %p2, align 4
  %match.result25 = alloca i32, align 4
  %x26 = extractvalue %Point.0 %p218, 0
  %x27 = alloca i32, align 4
  store i32 %x26, ptr %x27, align 4
  %y28 = extractvalue %Point.0 %p218, 1
  %y29 = alloca i32, align 4
  store i32 %y28, ptr %y29, align 4
  %x30 = load i32, ptr %x27, align 4
  %icmp31 = icmp sgt i32 %x30, 5
  br i1 %icmp31, label %match.arm20, label %match.next21

match.end19:                                      ; preds = %match.arm24, %match.arm22, %match.arm20
  %match.value42 = load i32, ptr %match.result25, align 4
  store i32 %match.value42, ptr %result2, align 4
  %result243 = load i32, ptr %result2, align 4
  %icmp44 = icmp ne i32 %result243, -6
  br i1 %icmp44, label %if.then45, label %if.end46

match.arm20:                                      ; preds = %if.end
  %x32 = load i32, ptr %x27, align 4
  %y33 = load i32, ptr %y29, align 4
  %add34 = add i32 %x32, %y33
  store i32 %add34, ptr %match.result25, align 4
  br label %match.end19

match.next21:                                     ; preds = %if.end
  br label %match.arm22

match.arm22:                                      ; preds = %match.next21
  %x35 = extractvalue %Point.0 %p218, 0
  %x36 = alloca i32, align 4
  store i32 %x35, ptr %x36, align 4
  %y37 = extractvalue %Point.0 %p218, 1
  %y38 = alloca i32, align 4
  store i32 %y37, ptr %y38, align 4
  %x39 = load i32, ptr %x36, align 4
  %y40 = load i32, ptr %y38, align 4
  %sub41 = sub i32 %x39, %y40
  store i32 %sub41, ptr %match.result25, align 4
  br label %match.end19

match.next23:                                     ; No predecessors!
  br label %match.arm24

match.arm24:                                      ; preds = %match.next23
  store i32 0, ptr %match.result25, align 4
  br label %match.end19

if.then45:                                        ; preds = %match.end19
  ret i32 2

if.end46:                                         ; preds = %match.end19
  ret i32 0
}

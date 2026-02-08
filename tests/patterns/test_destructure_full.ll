; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Rect = type { i32, i32 }
%Point.0 = type { i32, i32 }
%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result3 = alloca i32, align 4
  %r = alloca %Rect, align 8
  %result2 = alloca i32, align 4
  %p2 = alloca %Point.0, align 8
  %result1 = alloca i32, align 4
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %match.result = alloca i32, align 4
  br label %match.arm

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result1, align 4
  %result17 = load i32, ptr %result1, align 4
  %icmp = icmp ne i32 %result17, 30
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  %x = extractvalue %Point %p1, 0
  %x3 = alloca i32, align 4
  store i32 %x, ptr %x3, align 4
  %y = extractvalue %Point %p1, 1
  %y4 = alloca i32, align 4
  store i32 %y, ptr %y4, align 4
  %x5 = load i32, ptr %x3, align 4
  %y6 = load i32, ptr %y4, align 4
  %add = add i32 %x5, %y6
  store i32 %add, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; No predecessors!
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  store i32 0, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  store %Point.0 { i32 5, i32 15 }, ptr %p2, align 4
  %p28 = load %Point.0, ptr %p2, align 4
  %match.result13 = alloca i32, align 4
  br label %match.arm10

match.end9:                                       ; preds = %match.arm12, %match.arm10
  %match.value20 = load i32, ptr %match.result13, align 4
  store i32 %match.value20, ptr %result2, align 4
  %result221 = load i32, ptr %result2, align 4
  %icmp22 = icmp ne i32 %result221, 75
  br i1 %icmp22, label %if.then23, label %if.end24

match.arm10:                                      ; preds = %if.end
  %x14 = extractvalue %Point.0 %p28, 0
  %x15 = alloca i32, align 4
  store i32 %x14, ptr %x15, align 4
  %y16 = extractvalue %Point.0 %p28, 1
  %y17 = alloca i32, align 4
  store i32 %y16, ptr %y17, align 4
  %x18 = load i32, ptr %x15, align 4
  %y19 = load i32, ptr %y17, align 4
  %mul = mul i32 %x18, %y19
  store i32 %mul, ptr %match.result13, align 4
  br label %match.end9

match.next11:                                     ; No predecessors!
  br label %match.arm12

match.arm12:                                      ; preds = %match.next11
  store i32 0, ptr %match.result13, align 4
  br label %match.end9

if.then23:                                        ; preds = %match.end9
  ret i32 2

if.end24:                                         ; preds = %match.end9
  store %Rect { i32 8, i32 4 }, ptr %r, align 4
  %r25 = load %Rect, ptr %r, align 4
  %match.result30 = alloca i32, align 4
  br label %match.arm27

match.end26:                                      ; preds = %match.arm29, %match.arm27
  %match.value36 = load i32, ptr %match.result30, align 4
  store i32 %match.value36, ptr %result3, align 4
  %result337 = load i32, ptr %result3, align 4
  %icmp38 = icmp ne i32 %result337, 32
  br i1 %icmp38, label %if.then39, label %if.end40

match.arm27:                                      ; preds = %if.end24
  %width = extractvalue %Rect %r25, 0
  %width31 = alloca i32, align 4
  store i32 %width, ptr %width31, align 4
  %height = extractvalue %Rect %r25, 1
  %height32 = alloca i32, align 4
  store i32 %height, ptr %height32, align 4
  %width33 = load i32, ptr %width31, align 4
  %height34 = load i32, ptr %height32, align 4
  %mul35 = mul i32 %width33, %height34
  store i32 %mul35, ptr %match.result30, align 4
  br label %match.end26

match.next28:                                     ; No predecessors!
  br label %match.arm29

match.arm29:                                      ; preds = %match.next28
  store i32 0, ptr %match.result30, align 4
  br label %match.end26

if.then39:                                        ; preds = %match.end26
  ret i32 3

if.end40:                                         ; preds = %match.end26
  ret i32 0
}

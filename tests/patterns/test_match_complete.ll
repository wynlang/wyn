; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result3 = alloca i32, align 4
  %z = alloca i32, align 4
  %result2 = alloca i32, align 4
  %y = alloca i32, align 4
  %result = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 2, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %match.result = alloca i32, align 4
  %match.cmp = icmp eq i32 %x1, 1
  br i1 %match.cmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm6, %match.arm4, %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result9 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result9, 20
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 10, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  %match.cmp7 = icmp eq i32 %x1, 2
  br i1 %match.cmp7, label %match.arm2, label %match.next3

match.arm2:                                       ; preds = %match.next
  store i32 20, ptr %match.result, align 4
  br label %match.end

match.next3:                                      ; preds = %match.next
  %match.cmp8 = icmp eq i32 %x1, 3
  br i1 %match.cmp8, label %match.arm4, label %match.next5

match.arm4:                                       ; preds = %match.next3
  store i32 30, ptr %match.result, align 4
  br label %match.end

match.next5:                                      ; preds = %match.next3
  br label %match.arm6

match.arm6:                                       ; preds = %match.next5
  store i32 0, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  store i32 3, ptr %y, align 4
  %y10 = load i32, ptr %y, align 4
  %match.result19 = alloca i32, align 4
  %match.cmp20 = icmp eq i32 %y10, 1
  br i1 %match.cmp20, label %match.arm12, label %match.next13

match.end11:                                      ; preds = %match.arm18, %match.arm16, %match.arm14, %match.arm12
  %match.value23 = load i32, ptr %match.result19, align 4
  store i32 %match.value23, ptr %result2, align 4
  %result224 = load i32, ptr %result2, align 4
  %icmp25 = icmp ne i32 %result224, 30
  br i1 %icmp25, label %if.then26, label %if.end27

match.arm12:                                      ; preds = %if.end
  store i32 10, ptr %match.result19, align 4
  br label %match.end11

match.next13:                                     ; preds = %if.end
  %match.cmp21 = icmp eq i32 %y10, 2
  br i1 %match.cmp21, label %match.arm14, label %match.next15

match.arm14:                                      ; preds = %match.next13
  store i32 20, ptr %match.result19, align 4
  br label %match.end11

match.next15:                                     ; preds = %match.next13
  %match.cmp22 = icmp eq i32 %y10, 3
  br i1 %match.cmp22, label %match.arm16, label %match.next17

match.arm16:                                      ; preds = %match.next15
  store i32 30, ptr %match.result19, align 4
  br label %match.end11

match.next17:                                     ; preds = %match.next15
  br label %match.arm18

match.arm18:                                      ; preds = %match.next17
  store i32 0, ptr %match.result19, align 4
  br label %match.end11

if.then26:                                        ; preds = %match.end11
  ret i32 2

if.end27:                                         ; preds = %match.end11
  store i32 99, ptr %z, align 4
  %z28 = load i32, ptr %z, align 4
  %match.result37 = alloca i32, align 4
  %match.cmp38 = icmp eq i32 %z28, 1
  br i1 %match.cmp38, label %match.arm30, label %match.next31

match.end29:                                      ; preds = %match.arm36, %match.arm34, %match.arm32, %match.arm30
  %match.value41 = load i32, ptr %match.result37, align 4
  store i32 %match.value41, ptr %result3, align 4
  %result342 = load i32, ptr %result3, align 4
  %icmp43 = icmp ne i32 %result342, 0
  br i1 %icmp43, label %if.then44, label %if.end45

match.arm30:                                      ; preds = %if.end27
  store i32 10, ptr %match.result37, align 4
  br label %match.end29

match.next31:                                     ; preds = %if.end27
  %match.cmp39 = icmp eq i32 %z28, 2
  br i1 %match.cmp39, label %match.arm32, label %match.next33

match.arm32:                                      ; preds = %match.next31
  store i32 20, ptr %match.result37, align 4
  br label %match.end29

match.next33:                                     ; preds = %match.next31
  %match.cmp40 = icmp eq i32 %z28, 3
  br i1 %match.cmp40, label %match.arm34, label %match.next35

match.arm34:                                      ; preds = %match.next33
  store i32 30, ptr %match.result37, align 4
  br label %match.end29

match.next35:                                     ; preds = %match.next33
  br label %match.arm36

match.arm36:                                      ; preds = %match.next35
  store i32 0, ptr %match.result37, align 4
  br label %match.end29

if.then44:                                        ; preds = %match.end29
  ret i32 3

if.end45:                                         ; preds = %match.end29
  ret i32 0
}

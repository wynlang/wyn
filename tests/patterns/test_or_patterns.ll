; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result2 = alloca i32, align 4
  %y = alloca i32, align 4
  %result = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 2, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %match.result = alloca i32, align 4
  %or.cmp = icmp eq i32 %x1, 1
  %or.cmp3 = icmp eq i32 %x1, 2
  %or.result = or i1 %or.cmp, %or.cmp3
  %or.cmp4 = icmp eq i32 %x1, 3
  %or.result5 = or i1 %or.result, %or.cmp4
  br i1 %or.result5, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result6 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result6, 100
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 100, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  store i32 0, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  store i32 5, ptr %y, align 4
  %y7 = load i32, ptr %y, align 4
  %match.result12 = alloca i32, align 4
  %or.cmp13 = icmp eq i32 %y7, 1
  %or.cmp14 = icmp eq i32 %y7, 2
  %or.result15 = or i1 %or.cmp13, %or.cmp14
  %or.cmp16 = icmp eq i32 %y7, 3
  %or.result17 = or i1 %or.result15, %or.cmp16
  br i1 %or.result17, label %match.arm9, label %match.next10

match.end8:                                       ; preds = %match.arm11, %match.arm9
  %match.value18 = load i32, ptr %match.result12, align 4
  store i32 %match.value18, ptr %result2, align 4
  %result219 = load i32, ptr %result2, align 4
  %icmp20 = icmp ne i32 %result219, 0
  br i1 %icmp20, label %if.then21, label %if.end22

match.arm9:                                       ; preds = %if.end
  store i32 100, ptr %match.result12, align 4
  br label %match.end8

match.next10:                                     ; preds = %if.end
  br label %match.arm11

match.arm11:                                      ; preds = %match.next10
  store i32 0, ptr %match.result12, align 4
  br label %match.end8

if.then21:                                        ; preds = %match.end8
  ret i32 2

if.end22:                                         ; preds = %match.end8
  ret i32 0
}

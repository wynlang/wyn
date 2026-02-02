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
  %c2 = alloca i32, align 4
  %result = alloca i32, align 4
  %c = alloca i32, align 4
  %c1 = load i32, ptr %c, align 4
  %match.result = alloca i32, align 4
  br label %match.arm

match.end:                                        ; preds = %match.arm4, %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result5 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result5, 1
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; No predecessors!
  br label %match.arm2

match.arm2:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end

match.next3:                                      ; No predecessors!
  br label %match.arm4

match.arm4:                                       ; preds = %match.next3
  store i32 3, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  %c26 = load i32, ptr %c2, align 4
  %match.result13 = alloca i32, align 4
  br label %match.arm8

match.end7:                                       ; preds = %match.arm12, %match.arm10, %match.arm8
  %match.value14 = load i32, ptr %match.result13, align 4
  store i32 %match.value14, ptr %result2, align 4
  %result215 = load i32, ptr %result2, align 4
  %icmp16 = icmp ne i32 %result215, 2
  br i1 %icmp16, label %if.then17, label %if.end18

match.arm8:                                       ; preds = %if.end
  store i32 1, ptr %match.result13, align 4
  br label %match.end7

match.next9:                                      ; No predecessors!
  br label %match.arm10

match.arm10:                                      ; preds = %match.next9
  store i32 2, ptr %match.result13, align 4
  br label %match.end7

match.next11:                                     ; No predecessors!
  br label %match.arm12

match.arm12:                                      ; preds = %match.next11
  store i32 3, ptr %match.result13, align 4
  br label %match.end7

if.then17:                                        ; preds = %match.end7
  ret i32 2

if.end18:                                         ; preds = %match.end7
  ret i32 0
}

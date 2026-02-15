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
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %match.result = alloca i32, align 4
  %range.ge = icmp sge i32 %x1, 0
  %range.lt = icmp slt i32 %x1, 10
  %range.check = and i1 %range.ge, %range.lt
  br i1 %range.check, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result3 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result3, 100
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
  store i32 15, ptr %y, align 4
  %y4 = load i32, ptr %y, align 4
  %match.result9 = alloca i32, align 4
  %range.ge10 = icmp sge i32 %y4, 0
  %range.lt11 = icmp slt i32 %y4, 10
  %range.check12 = and i1 %range.ge10, %range.lt11
  br i1 %range.check12, label %match.arm6, label %match.next7

match.end5:                                       ; preds = %match.arm8, %match.arm6
  %match.value13 = load i32, ptr %match.result9, align 4
  store i32 %match.value13, ptr %result2, align 4
  %result214 = load i32, ptr %result2, align 4
  %icmp15 = icmp ne i32 %result214, 0
  br i1 %icmp15, label %if.then16, label %if.end17

match.arm6:                                       ; preds = %if.end
  store i32 100, ptr %match.result9, align 4
  br label %match.end5

match.next7:                                      ; preds = %if.end
  br label %match.arm8

match.arm8:                                       ; preds = %match.next7
  store i32 0, ptr %match.result9, align 4
  br label %match.end5

if.then16:                                        ; preds = %match.end5
  ret i32 2

if.end17:                                         ; preds = %match.end5
  ret i32 0
}

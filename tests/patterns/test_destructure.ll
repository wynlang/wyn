; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %match.result = alloca i32, align 4
  br label %match.arm

match.end:                                        ; preds = %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result7 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result7, 30
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
  ret i32 0
}

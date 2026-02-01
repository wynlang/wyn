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

match.end:                                        ; preds = %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result2 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result2, 30
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 30, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  ret i32 0
}

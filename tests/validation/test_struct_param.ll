; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Data = type { i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @get_value(i32 %d) {
entry:
  %d1 = alloca i32, align 4
  store i32 %d, ptr %d1, align 4
  %d2 = load i32, ptr %d1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %d = alloca %Data, align 8
  store %Data { i32 42 }, ptr %d, align 4
  %d1 = load %Data, ptr %d, align 4
  %get_value = call i32 @get_value(%Data %d1)
  store i32 %get_value, ptr %result, align 4
  %result2 = load i32, ptr %result, align 4
  %icmp = icmp eq i32 %result2, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 0

if.end:                                           ; preds = %entry
  ret i32 1
}

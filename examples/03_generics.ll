; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @identity(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  ret i32 %x2
}

define i32 @max(i32 %a, i32 %b) {
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

define i32 @wyn_main() {
entry:
  %larger = alloca i32, align 4
  %num = alloca i32, align 4
  %identity = call i32 @identity(i32 42)
  store i32 %identity, ptr %num, align 4
  %max = call i32 @max(i32 10, i32 20)
  store i32 %max, ptr %larger, align 4
  ret i32 0
}

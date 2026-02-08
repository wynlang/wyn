; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca i32, align 4
  %private_fn = call i32 @private_fn()
  store i32 %private_fn, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %icmp = icmp ne i32 %x1, 5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @public_fn() {
entry:
  %private_fn = call i32 @private_fn()
  %add = add i32 %private_fn, 10
  ret i32 %add
}

define i32 @private_fn() {
entry:
  ret i32 5
}

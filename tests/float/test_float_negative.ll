; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %x = alloca double, align 8
  store double -3.140000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %fcmp = fcmp ogt double %x1, -3.000000e+00
  br i1 %fcmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %x2 = load double, ptr %x, align 8
  %fcmp3 = fcmp olt double %x2, -4.000000e+00
  br i1 %fcmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  ret i32 0
}

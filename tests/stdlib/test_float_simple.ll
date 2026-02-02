; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %r = alloca i32, align 4
  %c = alloca i32, align 4
  %f = alloca i32, align 4
  %x = alloca double, align 8
  store double 3.700000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %f2 = load i32, ptr %f, align 4
  %fcmp = fcmp one i32 %f2, double 3.000000e+00
  br i1 %fcmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %x3 = load double, ptr %x, align 8
  %c4 = load i32, ptr %c, align 4
  %fcmp5 = fcmp one i32 %c4, double 4.000000e+00
  br i1 %fcmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  ret i32 2

if.end7:                                          ; preds = %if.end
  %x8 = load double, ptr %x, align 8
  %r9 = load i32, ptr %r, align 4
  %fcmp10 = fcmp one i32 %r9, double 4.000000e+00
  br i1 %fcmp10, label %if.then11, label %if.end12

if.then11:                                        ; preds = %if.end7
  ret i32 3

if.end12:                                         ; preds = %if.end7
  ret i32 0
}

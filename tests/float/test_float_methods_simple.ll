; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %r = alloca double, align 8
  %c = alloca double, align 8
  %f = alloca double, align 8
  %x = alloca double, align 8
  store double 3.700000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %floor = call double @floor(double %x1)
  store double %floor, ptr %f, align 8
  %f2 = load double, ptr %f, align 8
  %fcmp = fcmp one double %f2, 3.000000e+00
  br i1 %fcmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %x3 = load double, ptr %x, align 8
  %ceil = call double @ceil(double %x3)
  store double %ceil, ptr %c, align 8
  %c4 = load double, ptr %c, align 8
  %fcmp5 = fcmp one double %c4, 4.000000e+00
  br i1 %fcmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  ret i32 2

if.end7:                                          ; preds = %if.end
  %x8 = load double, ptr %x, align 8
  %round = call double @round(double %x8)
  store double %round, ptr %r, align 8
  %r9 = load double, ptr %r, align 8
  %fcmp10 = fcmp one double %r9, 4.000000e+00
  br i1 %fcmp10, label %if.then11, label %if.end12

if.then11:                                        ; preds = %if.end7
  ret i32 3

if.end12:                                         ; preds = %if.end7
  ret i32 0
}

declare double @floor(double)

declare double @ceil(double)

declare double @round(double)

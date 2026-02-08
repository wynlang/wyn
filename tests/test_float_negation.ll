; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %d = alloca double, align 8
  %c = alloca double, align 8
  %b = alloca double, align 8
  %a = alloca double, align 8
  %z = alloca double, align 8
  %y = alloca double, align 8
  %x = alloca double, align 8
  store double -3.140000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %fcmp = fcmp oge double %x1, 0.000000e+00
  br i1 %fcmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  store double 2.500000e+00, ptr %y, align 8
  %y2 = load double, ptr %y, align 8
  %fneg = fneg double %y2
  store double %fneg, ptr %z, align 8
  %z3 = load double, ptr %z, align 8
  %fcmp4 = fcmp oge double %z3, 0.000000e+00
  br i1 %fcmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  store double -5.500000e+00, ptr %a, align 8
  %a7 = load double, ptr %a, align 8
  %is_neg = fcmp olt double %a7, 0.000000e+00
  %neg = fneg double %a7
  %abs = select i1 %is_neg, double %neg, double %a7
  store double %abs, ptr %b, align 8
  %b8 = load double, ptr %b, align 8
  %fcmp9 = fcmp olt double %b8, 5.400000e+00
  br i1 %fcmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end6
  ret i32 3

if.end11:                                         ; preds = %if.end6
  %b12 = load double, ptr %b, align 8
  %fcmp13 = fcmp ogt double %b12, 5.600000e+00
  br i1 %fcmp13, label %if.then14, label %if.end15

if.then14:                                        ; preds = %if.end11
  ret i32 3

if.end15:                                         ; preds = %if.end11
  store double 7.700000e+00, ptr %c, align 8
  %c16 = load double, ptr %c, align 8
  %is_neg17 = fcmp olt double %c16, 0.000000e+00
  %neg18 = fneg double %c16
  %abs19 = select i1 %is_neg17, double %neg18, double %c16
  store double %abs19, ptr %d, align 8
  %d20 = load double, ptr %d, align 8
  %fcmp21 = fcmp olt double %d20, 7.600000e+00
  br i1 %fcmp21, label %if.then22, label %if.end23

if.then22:                                        ; preds = %if.end15
  ret i32 4

if.end23:                                         ; preds = %if.end15
  %d24 = load double, ptr %d, align 8
  %fcmp25 = fcmp ogt double %d24, 0x401F333333333333
  br i1 %fcmp25, label %if.then26, label %if.end27

if.then26:                                        ; preds = %if.end23
  ret i32 4

if.end27:                                         ; preds = %if.end23
  ret i32 0
}

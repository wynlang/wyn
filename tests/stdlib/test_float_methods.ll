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
  %a = alloca double, align 8
  %x = alloca double, align 8
  store double -3.140000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %is_neg = fcmp olt double %x1, 0.000000e+00
  %neg = fneg double %x1
  %abs = select i1 %is_neg, double %neg, double %x1
  store double %abs, ptr %a, align 8
  %a2 = load double, ptr %a, align 8
  %fcmp = fcmp olt double %a2, 3.130000e+00
  br i1 %fcmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %a3 = load double, ptr %a, align 8
  %fcmp4 = fcmp ogt double %a3, 3.150000e+00
  br i1 %fcmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 1

if.end6:                                          ; preds = %if.end
  %x7 = load double, ptr %x, align 8
  %floor = call double @llvm.floor.f64(double %x7)
  store double %floor, ptr %f, align 8
  %f8 = load double, ptr %f, align 8
  %fcmp9 = fcmp one double %f8, -4.000000e+00
  br i1 %fcmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end6
  ret i32 2

if.end11:                                         ; preds = %if.end6
  %x12 = load double, ptr %x, align 8
  %ceil = call double @llvm.ceil.f64(double %x12)
  store double %ceil, ptr %c, align 8
  %c13 = load double, ptr %c, align 8
  %fcmp14 = fcmp one double %c13, -3.000000e+00
  br i1 %fcmp14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end11
  ret i32 3

if.end16:                                         ; preds = %if.end11
  %x17 = load double, ptr %x, align 8
  %round = call double @llvm.round.f64(double %x17)
  store double %round, ptr %r, align 8
  %r18 = load double, ptr %r, align 8
  %fcmp19 = fcmp one double %r18, -3.000000e+00
  br i1 %fcmp19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end16
  ret i32 4

if.end21:                                         ; preds = %if.end16
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.floor.f64(double) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.ceil.f64(double) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.round.f64(double) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

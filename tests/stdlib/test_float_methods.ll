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
  %a = alloca double, align 8
  %x = alloca double, align 8
  store double -3.140000e+00, ptr %x, align 8
  %x1 = load double, ptr %x, align 8
  %is_neg = icmp slt double %x1, i32 0
  %neg = sub double 0.000000e+00, %x1
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
  %f8 = load i32, ptr %f, align 4
  %fcmp9 = fcmp one i32 %f8, double -4.000000e+00
  br i1 %fcmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end6
  ret i32 2

if.end11:                                         ; preds = %if.end6
  %x12 = load double, ptr %x, align 8
  %c13 = load i32, ptr %c, align 4
  %fcmp14 = fcmp one i32 %c13, double -3.000000e+00
  br i1 %fcmp14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end11
  ret i32 3

if.end16:                                         ; preds = %if.end11
  %x17 = load double, ptr %x, align 8
  %r18 = load i32, ptr %r, align 4
  %fcmp19 = fcmp one i32 %r18, double -3.000000e+00
  br i1 %fcmp19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end16
  ret i32 4

if.end21:                                         ; preds = %if.end16
  ret i32 0
}

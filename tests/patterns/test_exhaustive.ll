; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@RED = internal constant i32 0
@GREEN = internal constant i32 1
@BLUE = internal constant i32 2

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result2 = alloca i32, align 4
  %c2 = alloca i32, align 4
  %result = alloca i32, align 4
  %c = alloca i32, align 4
  %RED = load i32, ptr @RED, align 4
  store i32 %RED, ptr %c, align 4
  %c1 = load i32, ptr %c, align 4
  %match.result = alloca i32, align 4
  %RED5 = load i32, ptr @RED, align 4
  %match.cmp = icmp eq i32 %c1, %RED5
  br i1 %match.cmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm4, %match.next3, %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result8 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result8, 1
  br i1 %icmp, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  %GREEN = load i32, ptr @GREEN, align 4
  %match.cmp6 = icmp eq i32 %c1, %GREEN
  br i1 %match.cmp6, label %match.arm2, label %match.next3

match.arm2:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end

match.next3:                                      ; preds = %match.next
  %BLUE = load i32, ptr @BLUE, align 4
  %match.cmp7 = icmp eq i32 %c1, %BLUE
  br i1 %match.cmp7, label %match.arm4, label %match.end

match.arm4:                                       ; preds = %match.next3
  store i32 3, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  %GREEN9 = load i32, ptr @GREEN, align 4
  store i32 %GREEN9, ptr %c2, align 4
  %c210 = load i32, ptr %c2, align 4
  %match.result17 = alloca i32, align 4
  %RED18 = load i32, ptr @RED, align 4
  %match.cmp19 = icmp eq i32 %c210, %RED18
  br i1 %match.cmp19, label %match.arm12, label %match.next13

match.end11:                                      ; preds = %match.arm16, %match.next15, %match.arm14, %match.arm12
  %match.value24 = load i32, ptr %match.result17, align 4
  store i32 %match.value24, ptr %result2, align 4
  %result225 = load i32, ptr %result2, align 4
  %icmp26 = icmp ne i32 %result225, 2
  br i1 %icmp26, label %if.then27, label %if.end28

match.arm12:                                      ; preds = %if.end
  store i32 1, ptr %match.result17, align 4
  br label %match.end11

match.next13:                                     ; preds = %if.end
  %GREEN20 = load i32, ptr @GREEN, align 4
  %match.cmp21 = icmp eq i32 %c210, %GREEN20
  br i1 %match.cmp21, label %match.arm14, label %match.next15

match.arm14:                                      ; preds = %match.next13
  store i32 2, ptr %match.result17, align 4
  br label %match.end11

match.next15:                                     ; preds = %match.next13
  %BLUE22 = load i32, ptr @BLUE, align 4
  %match.cmp23 = icmp eq i32 %c210, %BLUE22
  br i1 %match.cmp23, label %match.arm16, label %match.end11

match.arm16:                                      ; preds = %match.next15
  store i32 3, ptr %match.result17, align 4
  br label %match.end11

if.then27:                                        ; preds = %match.end11
  ret i32 2

if.end28:                                         ; preds = %match.end11
  ret i32 0
}

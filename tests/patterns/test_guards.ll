; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result3 = alloca i32, align 4
  %z = alloca i32, align 4
  %result2 = alloca i32, align 4
  %y = alloca i32, align 4
  %result = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %match.result = alloca i32, align 4
  %n = alloca i32, align 4
  store i32 %x1, ptr %n, align 4
  %n5 = load i32, ptr %n, align 4
  %icmp = icmp sgt i32 %n5, 0
  br i1 %icmp, label %match.arm, label %match.next

match.end:                                        ; preds = %match.arm4, %match.arm2, %match.arm
  %match.value = load i32, ptr %match.result, align 4
  store i32 %match.value, ptr %result, align 4
  %result9 = load i32, ptr %result, align 4
  %icmp10 = icmp ne i32 %result9, 1
  br i1 %icmp10, label %if.then, label %if.end

match.arm:                                        ; preds = %entry
  store i32 1, ptr %match.result, align 4
  br label %match.end

match.next:                                       ; preds = %entry
  %n6 = alloca i32, align 4
  store i32 %x1, ptr %n6, align 4
  %n7 = load i32, ptr %n6, align 4
  %icmp8 = icmp slt i32 %n7, 0
  br i1 %icmp8, label %match.arm2, label %match.next3

match.arm2:                                       ; preds = %match.next
  store i32 2, ptr %match.result, align 4
  br label %match.end

match.next3:                                      ; preds = %match.next
  br label %match.arm4

match.arm4:                                       ; preds = %match.next3
  store i32 3, ptr %match.result, align 4
  br label %match.end

if.then:                                          ; preds = %match.end
  ret i32 1

if.end:                                           ; preds = %match.end
  store i32 -3, ptr %y, align 4
  %y11 = load i32, ptr %y, align 4
  %match.result18 = alloca i32, align 4
  %n19 = alloca i32, align 4
  store i32 %y11, ptr %n19, align 4
  %n20 = load i32, ptr %n19, align 4
  %icmp21 = icmp sgt i32 %n20, 0
  br i1 %icmp21, label %match.arm13, label %match.next14

match.end12:                                      ; preds = %match.arm17, %match.arm15, %match.arm13
  %match.value25 = load i32, ptr %match.result18, align 4
  store i32 %match.value25, ptr %result2, align 4
  %result226 = load i32, ptr %result2, align 4
  %icmp27 = icmp ne i32 %result226, 2
  br i1 %icmp27, label %if.then28, label %if.end29

match.arm13:                                      ; preds = %if.end
  store i32 1, ptr %match.result18, align 4
  br label %match.end12

match.next14:                                     ; preds = %if.end
  %n22 = alloca i32, align 4
  store i32 %y11, ptr %n22, align 4
  %n23 = load i32, ptr %n22, align 4
  %icmp24 = icmp slt i32 %n23, 0
  br i1 %icmp24, label %match.arm15, label %match.next16

match.arm15:                                      ; preds = %match.next14
  store i32 2, ptr %match.result18, align 4
  br label %match.end12

match.next16:                                     ; preds = %match.next14
  br label %match.arm17

match.arm17:                                      ; preds = %match.next16
  store i32 3, ptr %match.result18, align 4
  br label %match.end12

if.then28:                                        ; preds = %match.end12
  ret i32 2

if.end29:                                         ; preds = %match.end12
  store i32 0, ptr %z, align 4
  %z30 = load i32, ptr %z, align 4
  %match.result37 = alloca i32, align 4
  %n38 = alloca i32, align 4
  store i32 %z30, ptr %n38, align 4
  %n39 = load i32, ptr %n38, align 4
  %icmp40 = icmp sgt i32 %n39, 0
  br i1 %icmp40, label %match.arm32, label %match.next33

match.end31:                                      ; preds = %match.arm36, %match.arm34, %match.arm32
  %match.value44 = load i32, ptr %match.result37, align 4
  store i32 %match.value44, ptr %result3, align 4
  %result345 = load i32, ptr %result3, align 4
  %icmp46 = icmp ne i32 %result345, 3
  br i1 %icmp46, label %if.then47, label %if.end48

match.arm32:                                      ; preds = %if.end29
  store i32 1, ptr %match.result37, align 4
  br label %match.end31

match.next33:                                     ; preds = %if.end29
  %n41 = alloca i32, align 4
  store i32 %z30, ptr %n41, align 4
  %n42 = load i32, ptr %n41, align 4
  %icmp43 = icmp slt i32 %n42, 0
  br i1 %icmp43, label %match.arm34, label %match.next35

match.arm34:                                      ; preds = %match.next33
  store i32 2, ptr %match.result37, align 4
  br label %match.end31

match.next35:                                     ; preds = %match.next33
  br label %match.arm36

match.arm36:                                      ; preds = %match.next35
  store i32 3, ptr %match.result37, align 4
  br label %match.end31

if.then47:                                        ; preds = %match.end31
  ret i32 3

if.end48:                                         ; preds = %match.end31
  ret i32 0
}

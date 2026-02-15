; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Rectangle = type { i32, i32, i32 }
%Point.0 = type { i32, i32 }
%Point = type { i32, i32 }
%Point.1 = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %rect = alloca %Rectangle, align 8
  %p2 = alloca %Point.0, align 8
  %sum = alloca i32, align 4
  %p1 = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p1, align 4
  %p11 = load %Point, ptr %p1, align 4
  %field_val = extractvalue %Point %p11, 0
  %icmp = icmp ne i32 %field_val, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %p12 = load %Point, ptr %p1, align 4
  %field_val3 = extractvalue %Point %p12, 1
  %icmp4 = icmp ne i32 %field_val3, 20
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  %p17 = load %Point, ptr %p1, align 4
  %field_val8 = extractvalue %Point %p17, 0
  %p19 = load %Point, ptr %p1, align 4
  %field_val10 = extractvalue %Point %p19, 1
  %add = add i32 %field_val8, %field_val10
  store i32 %add, ptr %sum, align 4
  %sum11 = load i32, ptr %sum, align 4
  %icmp12 = icmp ne i32 %sum11, 30
  br i1 %icmp12, label %if.then13, label %if.end14

if.then13:                                        ; preds = %if.end6
  ret i32 3

if.end14:                                         ; preds = %if.end6
  store %Point.0 { i32 5, i32 15 }, ptr %p2, align 4
  %p215 = load %Point.0, ptr %p2, align 4
  %p116 = load %Point, ptr %p1, align 4
  %field_val17 = extractvalue %Point %p116, 0
  %p218 = load %Point.0, ptr %p2, align 4
  store %Rectangle { %Point.1 zeroinitializer, i32 100, i32 50 }, ptr %rect, align 4
  %rect19 = load %Rectangle, ptr %rect, align 4
  %field_val20 = extractvalue %Rectangle %rect19, 1
  %icmp21 = icmp ne i32 %field_val20, 100
  br i1 %icmp21, label %if.then22, label %if.end23

if.then22:                                        ; preds = %if.end14
  ret i32 6

if.end23:                                         ; preds = %if.end14
  %rect24 = load %Rectangle, ptr %rect, align 4
  %field_val25 = extractvalue %Rectangle %rect24, 2
  %icmp26 = icmp ne i32 %field_val25, 50
  br i1 %icmp26, label %if.then27, label %if.end28

if.then27:                                        ; preds = %if.end23
  ret i32 7

if.end28:                                         ; preds = %if.end23
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Color = type { i32, i32, i32 }
%Point.0 = type { i32, i32 }
%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %c = alloca %Color, align 8
  %p229 = alloca %Point.0, align 8
  %sum = alloca i32, align 4
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0
  %icmp = icmp ne i32 %field_val, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %p2 = load %Point, ptr %p, align 4
  %field_val3 = extractvalue %Point %p2, 1
  %icmp4 = icmp ne i32 %field_val3, 20
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  %p7 = load %Point, ptr %p, align 4
  %field_val8 = extractvalue %Point %p7, 0
  %p9 = load %Point, ptr %p, align 4
  %field_val10 = extractvalue %Point %p9, 1
  %add = add i32 %field_val8, %field_val10
  store i32 %add, ptr %sum, align 4
  %sum11 = load i32, ptr %sum, align 4
  %icmp12 = icmp ne i32 %sum11, 30
  br i1 %icmp12, label %if.then13, label %if.end14

if.then13:                                        ; preds = %if.end6
  ret i32 3

if.end14:                                         ; preds = %if.end6
  %p15 = load %Point, ptr %p, align 4
  %field_val16 = extractvalue %Point %p15, 0
  %p17 = load %Point, ptr %p, align 4
  %field_val18 = extractvalue %Point %p17, 1
  %icmp19 = icmp sge i32 %field_val16, %field_val18
  br i1 %icmp19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end14
  ret i32 4

if.end21:                                         ; preds = %if.end14
  %p22 = load %Point, ptr %p, align 4
  %field_val23 = extractvalue %Point %p22, 1
  %p24 = load %Point, ptr %p, align 4
  %field_val25 = extractvalue %Point %p24, 0
  %icmp26 = icmp sle i32 %field_val23, %field_val25
  br i1 %icmp26, label %if.then27, label %if.end28

if.then27:                                        ; preds = %if.end21
  ret i32 5

if.end28:                                         ; preds = %if.end21
  store %Point.0 { i32 5, i32 15 }, ptr %p229, align 4
  %p30 = load %Point, ptr %p, align 4
  %field_val31 = extractvalue %Point %p30, 0
  %p232 = load %Point.0, ptr %p229, align 4
  store %Color { i32 255, i32 128, i32 64 }, ptr %c, align 4
  %c33 = load %Color, ptr %c, align 4
  %field_val34 = extractvalue %Color %c33, 0
  %icmp35 = icmp ne i32 %field_val34, 255
  br i1 %icmp35, label %if.then36, label %if.end37

if.then36:                                        ; preds = %if.end28
  ret i32 7

if.end37:                                         ; preds = %if.end28
  %c38 = load %Color, ptr %c, align 4
  %field_val39 = extractvalue %Color %c38, 1
  %icmp40 = icmp ne i32 %field_val39, 128
  br i1 %icmp40, label %if.then41, label %if.end42

if.then41:                                        ; preds = %if.end37
  ret i32 8

if.end42:                                         ; preds = %if.end37
  %c43 = load %Color, ptr %c, align 4
  %field_val44 = extractvalue %Color %c43, 2
  %icmp45 = icmp ne i32 %field_val44, 64
  br i1 %icmp45, label %if.then46, label %if.end47

if.then46:                                        ; preds = %if.end42
  ret i32 9

if.end47:                                         ; preds = %if.end42
  ret i32 0
}

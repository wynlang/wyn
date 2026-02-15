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
  %result = alloca i32, align 4
  %c = alloca %Color, align 8
  %p2 = alloca %Point.0, align 8
  %sum = alloca i32, align 4
  %p = alloca %Point, align 8
  %tests_failed = alloca i32, align 4
  %tests_passed = alloca i32, align 4
  store i32 0, ptr %tests_passed, align 4
  store i32 0, ptr %tests_failed, align 4
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0
  %icmp = icmp eq i32 %field_val, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %tests_passed2 = load i32, ptr %tests_passed, align 4
  %add = add i32 %tests_passed2, 1
  store i32 %add, ptr %tests_passed, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %p3 = load %Point, ptr %p, align 4
  %field_val4 = extractvalue %Point %p3, 1
  %icmp5 = icmp eq i32 %field_val4, 20
  br i1 %icmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  %tests_passed8 = load i32, ptr %tests_passed, align 4
  %add9 = add i32 %tests_passed8, 1
  store i32 %add9, ptr %tests_passed, align 4
  br label %if.end7

if.end7:                                          ; preds = %if.then6, %if.end
  %p10 = load %Point, ptr %p, align 4
  %field_val11 = extractvalue %Point %p10, 0
  %p12 = load %Point, ptr %p, align 4
  %field_val13 = extractvalue %Point %p12, 1
  %add14 = add i32 %field_val11, %field_val13
  store i32 %add14, ptr %sum, align 4
  %sum15 = load i32, ptr %sum, align 4
  %icmp16 = icmp eq i32 %sum15, 30
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end7
  %tests_passed19 = load i32, ptr %tests_passed, align 4
  %add20 = add i32 %tests_passed19, 1
  store i32 %add20, ptr %tests_passed, align 4
  br label %if.end18

if.end18:                                         ; preds = %if.then17, %if.end7
  store %Point.0 { i32 5, i32 15 }, ptr %p2, align 4
  %p221 = load %Point.0, ptr %p2, align 4
  %p22 = load %Point, ptr %p, align 4
  %field_val23 = extractvalue %Point %p22, 0
  %p224 = load %Point.0, ptr %p2, align 4
  store %Color { i32 255, i32 128, i32 64 }, ptr %c, align 4
  %c25 = load %Color, ptr %c, align 4
  %field_val26 = extractvalue %Color %c25, 0
  %icmp27 = icmp eq i32 %field_val26, 255
  br i1 %icmp27, label %if.then28, label %if.end29

if.then28:                                        ; preds = %if.end18
  %tests_passed30 = load i32, ptr %tests_passed, align 4
  %add31 = add i32 %tests_passed30, 1
  store i32 %add31, ptr %tests_passed, align 4
  br label %if.end29

if.end29:                                         ; preds = %if.then28, %if.end18
  %c32 = load %Color, ptr %c, align 4
  %field_val33 = extractvalue %Color %c32, 1
  %icmp34 = icmp eq i32 %field_val33, 128
  br i1 %icmp34, label %if.then35, label %if.end36

if.then35:                                        ; preds = %if.end29
  %tests_passed37 = load i32, ptr %tests_passed, align 4
  %add38 = add i32 %tests_passed37, 1
  store i32 %add38, ptr %tests_passed, align 4
  br label %if.end36

if.end36:                                         ; preds = %if.then35, %if.end29
  %c39 = load %Color, ptr %c, align 4
  %field_val40 = extractvalue %Color %c39, 2
  %icmp41 = icmp eq i32 %field_val40, 64
  br i1 %icmp41, label %if.then42, label %if.end43

if.then42:                                        ; preds = %if.end36
  %tests_passed44 = load i32, ptr %tests_passed, align 4
  %add45 = add i32 %tests_passed44, 1
  store i32 %add45, ptr %tests_passed, align 4
  br label %if.end43

if.end43:                                         ; preds = %if.then42, %if.end36
  %p46 = load %Point, ptr %p, align 4
  %field_val47 = extractvalue %Point %p46, 0
  %icmp48 = icmp slt i32 %field_val47, 20
  br i1 %icmp48, label %if.then49, label %if.end50

if.then49:                                        ; preds = %if.end43
  %p51 = load %Point, ptr %p, align 4
  %field_val52 = extractvalue %Point %p51, 1
  %icmp53 = icmp sgt i32 %field_val52, 10
  br i1 %icmp53, label %if.then54, label %if.end55

if.end50:                                         ; preds = %if.end55, %if.end43
  %p58 = load %Point, ptr %p, align 4
  %field_val59 = extractvalue %Point %p58, 0
  %mul = mul i32 %field_val59, 2
  %p60 = load %Point, ptr %p, align 4
  %field_val61 = extractvalue %Point %p60, 1
  %div = sdiv i32 %field_val61, 2
  %add62 = add i32 %mul, %div
  store i32 %add62, ptr %result, align 4
  %result63 = load i32, ptr %result, align 4
  %icmp64 = icmp eq i32 %result63, 30
  br i1 %icmp64, label %if.then65, label %if.end66

if.then54:                                        ; preds = %if.then49
  %tests_passed56 = load i32, ptr %tests_passed, align 4
  %add57 = add i32 %tests_passed56, 1
  store i32 %add57, ptr %tests_passed, align 4
  br label %if.end55

if.end55:                                         ; preds = %if.then54, %if.then49
  br label %if.end50

if.then65:                                        ; preds = %if.end50
  %tests_passed67 = load i32, ptr %tests_passed, align 4
  %add68 = add i32 %tests_passed67, 1
  store i32 %add68, ptr %tests_passed, align 4
  br label %if.end66

if.end66:                                         ; preds = %if.then65, %if.end50
  %tests_passed69 = load i32, ptr %tests_passed, align 4
  %icmp70 = icmp eq i32 %tests_passed69, 10
  br i1 %icmp70, label %if.then71, label %if.end72

if.then71:                                        ; preds = %if.end66
  ret i32 0

if.end72:                                         ; preds = %if.end66
  %tests_passed73 = load i32, ptr %tests_passed, align 4
  ret i32 %tests_passed73
}

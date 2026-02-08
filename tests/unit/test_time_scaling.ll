; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @days_in_month(i32 %month) {
entry:
  %month1 = alloca i32, align 4
  store i32 %month, ptr %month1, align 4
  %month2 = load i32, ptr %month1, align 4
  %icmp = icmp eq i32 %month2, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 31

if.end:                                           ; preds = %entry
  %month3 = load i32, ptr %month1, align 4
  %icmp4 = icmp eq i32 %month3, 2
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 28

if.end6:                                          ; preds = %if.end
  %month7 = load i32, ptr %month1, align 4
  %icmp8 = icmp eq i32 %month7, 3
  br i1 %icmp8, label %if.then9, label %if.end10

if.then9:                                         ; preds = %if.end6
  ret i32 31

if.end10:                                         ; preds = %if.end6
  %month11 = load i32, ptr %month1, align 4
  %icmp12 = icmp eq i32 %month11, 4
  br i1 %icmp12, label %if.then13, label %if.end14

if.then13:                                        ; preds = %if.end10
  ret i32 30

if.end14:                                         ; preds = %if.end10
  %month15 = load i32, ptr %month1, align 4
  %icmp16 = icmp eq i32 %month15, 5
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end14
  ret i32 31

if.end18:                                         ; preds = %if.end14
  %month19 = load i32, ptr %month1, align 4
  %icmp20 = icmp eq i32 %month19, 6
  br i1 %icmp20, label %if.then21, label %if.end22

if.then21:                                        ; preds = %if.end18
  ret i32 30

if.end22:                                         ; preds = %if.end18
  %month23 = load i32, ptr %month1, align 4
  %icmp24 = icmp eq i32 %month23, 7
  br i1 %icmp24, label %if.then25, label %if.end26

if.then25:                                        ; preds = %if.end22
  ret i32 31

if.end26:                                         ; preds = %if.end22
  %month27 = load i32, ptr %month1, align 4
  %icmp28 = icmp eq i32 %month27, 8
  br i1 %icmp28, label %if.then29, label %if.end30

if.then29:                                        ; preds = %if.end26
  ret i32 31

if.end30:                                         ; preds = %if.end26
  %month31 = load i32, ptr %month1, align 4
  %icmp32 = icmp eq i32 %month31, 9
  br i1 %icmp32, label %if.then33, label %if.end34

if.then33:                                        ; preds = %if.end30
  ret i32 30

if.end34:                                         ; preds = %if.end30
  %month35 = load i32, ptr %month1, align 4
  %icmp36 = icmp eq i32 %month35, 10
  br i1 %icmp36, label %if.then37, label %if.end38

if.then37:                                        ; preds = %if.end34
  ret i32 31

if.end38:                                         ; preds = %if.end34
  %month39 = load i32, ptr %month1, align 4
  %icmp40 = icmp eq i32 %month39, 11
  br i1 %icmp40, label %if.then41, label %if.end42

if.then41:                                        ; preds = %if.end38
  ret i32 30

if.end42:                                         ; preds = %if.end38
  %month43 = load i32, ptr %month1, align 4
  %icmp44 = icmp eq i32 %month43, 12
  br i1 %icmp44, label %if.then45, label %if.end46

if.then45:                                        ; preds = %if.end42
  ret i32 31

if.end46:                                         ; preds = %if.end42
  ret i32 0
}

define i32 @is_leap_year(i32 %year) {
entry:
  %div400 = alloca i32, align 4
  %div100 = alloca i32, align 4
  %div4 = alloca i32, align 4
  %year1 = alloca i32, align 4
  store i32 %year, ptr %year1, align 4
  %year2 = load i32, ptr %year1, align 4
  %rem = srem i32 %year2, 4
  store i32 %rem, ptr %div4, align 4
  %year3 = load i32, ptr %year1, align 4
  %rem4 = srem i32 %year3, 100
  store i32 %rem4, ptr %div100, align 4
  %year5 = load i32, ptr %year1, align 4
  %rem6 = srem i32 %year5, 400
  store i32 %rem6, ptr %div400, align 4
  %div4007 = load i32, ptr %div400, align 4
  %icmp = icmp eq i32 %div4007, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %div1008 = load i32, ptr %div100, align 4
  %icmp9 = icmp eq i32 %div1008, 0
  br i1 %icmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end
  ret i32 0

if.end11:                                         ; preds = %if.end
  %div412 = load i32, ptr %div4, align 4
  %icmp13 = icmp eq i32 %div412, 0
  br i1 %icmp13, label %if.then14, label %if.end15

if.then14:                                        ; preds = %if.end11
  ret i32 1

if.end15:                                         ; preds = %if.end11
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %m12 = alloca i32, align 4
  %m11 = alloca i32, align 4
  %m10 = alloca i32, align 4
  %m9 = alloca i32, align 4
  %m8 = alloca i32, align 4
  %m7 = alloca i32, align 4
  %m6 = alloca i32, align 4
  %m5 = alloca i32, align 4
  %m4 = alloca i32, align 4
  %m3 = alloca i32, align 4
  %m2 = alloca i32, align 4
  %m1 = alloca i32, align 4
  %y10 = alloca i32, align 4
  %y9 = alloca i32, align 4
  %y8 = alloca i32, align 4
  %y7 = alloca i32, align 4
  %y6 = alloca i32, align 4
  %y5 = alloca i32, align 4
  %y4 = alloca i32, align 4
  %y3 = alloca i32, align 4
  %y2 = alloca i32, align 4
  %y1 = alloca i32, align 4
  %is_leap_year = call i32 @is_leap_year(i32 2000)
  store i32 %is_leap_year, ptr %y1, align 4
  %is_leap_year1 = call i32 @is_leap_year(i32 2004)
  store i32 %is_leap_year1, ptr %y2, align 4
  %is_leap_year2 = call i32 @is_leap_year(i32 2008)
  store i32 %is_leap_year2, ptr %y3, align 4
  %is_leap_year3 = call i32 @is_leap_year(i32 2012)
  store i32 %is_leap_year3, ptr %y4, align 4
  %is_leap_year4 = call i32 @is_leap_year(i32 2016)
  store i32 %is_leap_year4, ptr %y5, align 4
  %is_leap_year5 = call i32 @is_leap_year(i32 2020)
  store i32 %is_leap_year5, ptr %y6, align 4
  %is_leap_year6 = call i32 @is_leap_year(i32 2024)
  store i32 %is_leap_year6, ptr %y7, align 4
  %is_leap_year7 = call i32 @is_leap_year(i32 2001)
  store i32 %is_leap_year7, ptr %y8, align 4
  %is_leap_year8 = call i32 @is_leap_year(i32 2002)
  store i32 %is_leap_year8, ptr %y9, align 4
  %is_leap_year9 = call i32 @is_leap_year(i32 2003)
  store i32 %is_leap_year9, ptr %y10, align 4
  %days_in_month = call i32 @days_in_month(i32 1)
  store i32 %days_in_month, ptr %m1, align 4
  %days_in_month10 = call i32 @days_in_month(i32 2)
  store i32 %days_in_month10, ptr %m2, align 4
  %days_in_month11 = call i32 @days_in_month(i32 3)
  store i32 %days_in_month11, ptr %m3, align 4
  %days_in_month12 = call i32 @days_in_month(i32 4)
  store i32 %days_in_month12, ptr %m4, align 4
  %days_in_month13 = call i32 @days_in_month(i32 5)
  store i32 %days_in_month13, ptr %m5, align 4
  %days_in_month14 = call i32 @days_in_month(i32 6)
  store i32 %days_in_month14, ptr %m6, align 4
  %days_in_month15 = call i32 @days_in_month(i32 7)
  store i32 %days_in_month15, ptr %m7, align 4
  %days_in_month16 = call i32 @days_in_month(i32 8)
  store i32 %days_in_month16, ptr %m8, align 4
  %days_in_month17 = call i32 @days_in_month(i32 9)
  store i32 %days_in_month17, ptr %m9, align 4
  %days_in_month18 = call i32 @days_in_month(i32 10)
  store i32 %days_in_month18, ptr %m10, align 4
  %days_in_month19 = call i32 @days_in_month(i32 11)
  store i32 %days_in_month19, ptr %m11, align 4
  %days_in_month20 = call i32 @days_in_month(i32 12)
  store i32 %days_in_month20, ptr %m12, align 4
  %y121 = load i32, ptr %y1, align 4
  %y222 = load i32, ptr %y2, align 4
  %add = add i32 %y121, %y222
  %y323 = load i32, ptr %y3, align 4
  %add24 = add i32 %add, %y323
  %y425 = load i32, ptr %y4, align 4
  %add26 = add i32 %add24, %y425
  %y527 = load i32, ptr %y5, align 4
  %add28 = add i32 %add26, %y527
  %y629 = load i32, ptr %y6, align 4
  %add30 = add i32 %add28, %y629
  %y731 = load i32, ptr %y7, align 4
  %add32 = add i32 %add30, %y731
  %y833 = load i32, ptr %y8, align 4
  %add34 = add i32 %add32, %y833
  %y935 = load i32, ptr %y9, align 4
  %add36 = add i32 %add34, %y935
  %y1037 = load i32, ptr %y10, align 4
  %add38 = add i32 %add36, %y1037
  %m139 = load i32, ptr %m1, align 4
  %add40 = add i32 %add38, %m139
  %m241 = load i32, ptr %m2, align 4
  %add42 = add i32 %add40, %m241
  %m343 = load i32, ptr %m3, align 4
  %add44 = add i32 %add42, %m343
  %m445 = load i32, ptr %m4, align 4
  %add46 = add i32 %add44, %m445
  %m547 = load i32, ptr %m5, align 4
  %add48 = add i32 %add46, %m547
  %m649 = load i32, ptr %m6, align 4
  %add50 = add i32 %add48, %m649
  %m751 = load i32, ptr %m7, align 4
  %add52 = add i32 %add50, %m751
  %m853 = load i32, ptr %m8, align 4
  %add54 = add i32 %add52, %m853
  %m955 = load i32, ptr %m9, align 4
  %add56 = add i32 %add54, %m955
  %m1057 = load i32, ptr %m10, align 4
  %add58 = add i32 %add56, %m1057
  %m1159 = load i32, ptr %m11, align 4
  %add60 = add i32 %add58, %m1159
  %m1261 = load i32, ptr %m12, align 4
  %add62 = add i32 %add60, %m1261
  ret i32 %add62
}

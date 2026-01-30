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

define i32 @hours_to_seconds(i32 %hours) {
entry:
  %hours1 = alloca i32, align 4
  store i32 %hours, ptr %hours1, align 4
  %hours2 = load i32, ptr %hours1, align 4
  %mul = mul i32 %hours2, 3600
  ret i32 %mul
}

define i32 @minutes_to_seconds(i32 %minutes) {
entry:
  %minutes1 = alloca i32, align 4
  store i32 %minutes, ptr %minutes1, align 4
  %minutes2 = load i32, ptr %minutes1, align 4
  %mul = mul i32 %minutes2, 60
  ret i32 %mul
}

define i32 @wyn_main() {
entry:
  %m2s = alloca i32, align 4
  %h2s = alloca i32, align 4
  %leap_2023 = alloca i32, align 4
  %leap_1900 = alloca i32, align 4
  %leap_2000 = alloca i32, align 4
  %leap_2024 = alloca i32, align 4
  %dec = alloca i32, align 4
  %apr = alloca i32, align 4
  %feb = alloca i32, align 4
  %jan = alloca i32, align 4
  %days_in_month = call i32 @days_in_month(i32 1)
  store i32 %days_in_month, ptr %jan, align 4
  %days_in_month1 = call i32 @days_in_month(i32 2)
  store i32 %days_in_month1, ptr %feb, align 4
  %days_in_month2 = call i32 @days_in_month(i32 4)
  store i32 %days_in_month2, ptr %apr, align 4
  %days_in_month3 = call i32 @days_in_month(i32 12)
  store i32 %days_in_month3, ptr %dec, align 4
  %is_leap_year = call i32 @is_leap_year(i32 2024)
  store i32 %is_leap_year, ptr %leap_2024, align 4
  %is_leap_year4 = call i32 @is_leap_year(i32 2000)
  store i32 %is_leap_year4, ptr %leap_2000, align 4
  %is_leap_year5 = call i32 @is_leap_year(i32 1900)
  store i32 %is_leap_year5, ptr %leap_1900, align 4
  %is_leap_year6 = call i32 @is_leap_year(i32 2023)
  store i32 %is_leap_year6, ptr %leap_2023, align 4
  %hours_to_seconds = call i32 @hours_to_seconds(i32 1)
  store i32 %hours_to_seconds, ptr %h2s, align 4
  %minutes_to_seconds = call i32 @minutes_to_seconds(i32 1)
  store i32 %minutes_to_seconds, ptr %m2s, align 4
  %jan7 = load i32, ptr %jan, align 4
  %feb8 = load i32, ptr %feb, align 4
  %add = add i32 %jan7, %feb8
  %apr9 = load i32, ptr %apr, align 4
  %add10 = add i32 %add, %apr9
  %dec11 = load i32, ptr %dec, align 4
  %add12 = add i32 %add10, %dec11
  %leap_202413 = load i32, ptr %leap_2024, align 4
  %add14 = add i32 %add12, %leap_202413
  %leap_200015 = load i32, ptr %leap_2000, align 4
  %add16 = add i32 %add14, %leap_200015
  %leap_190017 = load i32, ptr %leap_1900, align 4
  %add18 = add i32 %add16, %leap_190017
  %leap_202319 = load i32, ptr %leap_2023, align 4
  %add20 = add i32 %add18, %leap_202319
  ret i32 %add20
}

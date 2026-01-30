; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @is_leap_year(i32 %year) {
entry:
  %div4 = alloca i32, align 4
  %year1 = alloca i32, align 4
  store i32 %year, ptr %year1, align 4
  %year2 = load i32, ptr %year1, align 4
  %rem = srem i32 %year2, 4
  store i32 %rem, ptr %div4, align 4
  %div43 = load i32, ptr %div4, align 4
  %icmp = icmp eq i32 %div43, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %d = alloca i32, align 4
  %c = alloca i32, align 4
  %b = alloca i32, align 4
  %a = alloca i32, align 4
  %is_leap_year = call i32 @is_leap_year(i32 2024)
  store i32 %is_leap_year, ptr %a, align 4
  %is_leap_year1 = call i32 @is_leap_year(i32 2020)
  store i32 %is_leap_year1, ptr %b, align 4
  %is_leap_year2 = call i32 @is_leap_year(i32 2023)
  store i32 %is_leap_year2, ptr %c, align 4
  %is_leap_year3 = call i32 @is_leap_year(i32 2021)
  store i32 %is_leap_year3, ptr %d, align 4
  %a4 = load i32, ptr %a, align 4
  %b5 = load i32, ptr %b, align 4
  %add = add i32 %a4, %b5
  %c6 = load i32, ptr %c, align 4
  %add7 = add i32 %add, %c6
  %d8 = load i32, ptr %d, align 4
  %add9 = add i32 %add7, %d8
  ret i32 %add9
}

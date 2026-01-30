; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"apple\00", align 1
@str.1 = private unnamed_addr constant [7 x i8] c"banana\00", align 1
@str.2 = private unnamed_addr constant [8 x i8] c"missing\00", align 1
@str.3 = private unnamed_addr constant [5 x i8] c"date\00", align 1
@str.4 = private unnamed_addr constant [7 x i8] c"banana\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %set = alloca i32, align 4
  %set1 = load i32, ptr %set, align 4
  %not = xor i32 %set1, -1
  %strstr_result = call ptr @strstr(i32 %not, ptr @str)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  %tobool = icmp ne i32 %contains, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %set2 = load i32, ptr %set, align 4
  %not3 = xor i32 %set2, -1
  %strstr_result4 = call ptr @strstr(i32 %not3, ptr @str.1)
  %is_found5 = icmp ne ptr %strstr_result4, null
  %contains6 = zext i1 %is_found5 to i32
  %tobool7 = icmp ne i32 %contains6, 0
  br i1 %tobool7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end
  ret i32 2

if.end9:                                          ; preds = %if.end
  %set10 = load i32, ptr %set, align 4
  %strstr_result11 = call ptr @strstr(i32 %set10, ptr @str.2)
  %is_found12 = icmp ne ptr %strstr_result11, null
  %contains13 = zext i1 %is_found12 to i32
  %tobool14 = icmp ne i32 %contains13, 0
  br i1 %tobool14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end9
  ret i32 3

if.end16:                                         ; preds = %if.end9
  %set17 = load i32, ptr %set, align 4
  %set18 = load i32, ptr %set, align 4
  %not19 = xor i32 %set18, -1
  %strstr_result20 = call ptr @strstr(i32 %not19, ptr @str.3)
  %is_found21 = icmp ne ptr %strstr_result20, null
  %contains22 = zext i1 %is_found21 to i32
  %tobool23 = icmp ne i32 %contains22, 0
  br i1 %tobool23, label %if.then24, label %if.end25

if.then24:                                        ; preds = %if.end16
  ret i32 4

if.end25:                                         ; preds = %if.end16
  %set26 = load i32, ptr %set, align 4
  %set27 = load i32, ptr %set, align 4
  %strstr_result28 = call ptr @strstr(i32 %set27, ptr @str.4)
  %is_found29 = icmp ne ptr %strstr_result28, null
  %contains30 = zext i1 %is_found29 to i32
  %tobool31 = icmp ne i32 %contains30, 0
  br i1 %tobool31, label %if.then32, label %if.end33

if.then32:                                        ; preds = %if.end25
  ret i32 5

if.end33:                                         ; preds = %if.end25
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

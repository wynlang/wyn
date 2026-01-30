; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [46 x i8] c"\E2\95\91   WYN - OBJECT-ORIENTED SYNTAX        \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [19 x i8] c"1. String Methods:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [16 x i8] c"Wyn is awesome!\00", align 1
@str.14 = private unnamed_addr constant [4 x i8] c"Wyn\00", align 1
@str.15 = private unnamed_addr constant [32 x i8] c"   message.contains(\\\22Wyn\\\22) = \00", align 1
@fmt.16 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.18 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.19 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.20 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.21 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.22 = private unnamed_addr constant [22 x i8] c"2. OOP vs Functional:\00", align 1
@fmt.23 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.24 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.25 = private unnamed_addr constant [41 x i8] c"   \E2\9D\8C Old: str_contains(text, \\\22word\\\22)\00", align 1
@fmt.26 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.27 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.28 = private unnamed_addr constant [36 x i8] c"   \E2\9C\85 New: text.contains(\\\22word\\\22)\00", align 1
@fmt.29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.30 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.31 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.33 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.34 = private unnamed_addr constant [22 x i8] c"   \E2\9D\8C Old: len(text)\00", align 1
@fmt.35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.36 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.37 = private unnamed_addr constant [26 x i8] c"   \E2\9C\85 New: text.length()\00", align 1
@fmt.38 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.39 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.40 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.42 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.43 = private unnamed_addr constant [25 x i8] c"3. Clean, Readable Code:\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.46 = private unnamed_addr constant [17 x i8] c"user@example.com\00", align 1
@str.47 = private unnamed_addr constant [2 x i8] c"@\00", align 1
@str.48 = private unnamed_addr constant [26 x i8] c"   \E2\9C\93 Valid email format\00", align 1
@fmt.49 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.50 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.51 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.52 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.53 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.54 = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt.55 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.56 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.57 = private unnamed_addr constant [48 x i8] c"\E2\95\91   WYN IS OBJECT-ORIENTED! \E2\9C\93           \E2\95\91\00", align 1
@fmt.58 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.59 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.60 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.61 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.62 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %email = alloca ptr, align 8
  %has_wyn = alloca i32, align 4
  %message = alloca ptr, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @fmt, ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @nl)
  %2 = call i32 (ptr, ...) @printf(ptr @fmt.2, ptr @str.1)
  %3 = call i32 (ptr, ...) @printf(ptr @nl.3)
  %4 = call i32 (ptr, ...) @printf(ptr @fmt.5, ptr @str.4)
  %5 = call i32 (ptr, ...) @printf(ptr @nl.6)
  %6 = call i32 (ptr, ...) @printf(ptr @fmt.8, ptr @str.7)
  %7 = call i32 (ptr, ...) @printf(ptr @nl.9)
  %8 = call i32 (ptr, ...) @printf(ptr @fmt.11, ptr @str.10)
  %9 = call i32 (ptr, ...) @printf(ptr @nl.12)
  store ptr @str.13, ptr %message, align 8
  %message1 = load ptr, ptr %message, align 8
  %strstr_result = call ptr @strstr(ptr %message1, ptr @str.14)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_wyn, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.16, ptr @str.15)
  %has_wyn2 = load i32, ptr %has_wyn, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.17, i32 %has_wyn2)
  %12 = call i32 (ptr, ...) @printf(ptr @nl.18)
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.20, ptr @str.19)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.21)
  %15 = call i32 (ptr, ...) @printf(ptr @fmt.23, ptr @str.22)
  %16 = call i32 (ptr, ...) @printf(ptr @nl.24)
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.26, ptr @str.25)
  %18 = call i32 (ptr, ...) @printf(ptr @nl.27)
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.29, ptr @str.28)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.30)
  %21 = call i32 (ptr, ...) @printf(ptr @fmt.32, ptr @str.31)
  %22 = call i32 (ptr, ...) @printf(ptr @nl.33)
  %23 = call i32 (ptr, ...) @printf(ptr @fmt.35, ptr @str.34)
  %24 = call i32 (ptr, ...) @printf(ptr @nl.36)
  %25 = call i32 (ptr, ...) @printf(ptr @fmt.38, ptr @str.37)
  %26 = call i32 (ptr, ...) @printf(ptr @nl.39)
  %27 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @str.40)
  %28 = call i32 (ptr, ...) @printf(ptr @nl.42)
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %30 = call i32 (ptr, ...) @printf(ptr @nl.45)
  store ptr @str.46, ptr %email, align 8
  %email3 = load ptr, ptr %email, align 8
  %strstr_result4 = call ptr @strstr(ptr %email3, ptr @str.47)
  %is_found5 = icmp ne ptr %strstr_result4, null
  %contains6 = zext i1 %is_found5 to i32
  %tobool = icmp ne i32 %contains6, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %31 = call i32 (ptr, ...) @printf(ptr @fmt.49, ptr @str.48)
  %32 = call i32 (ptr, ...) @printf(ptr @nl.50)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.52, ptr @str.51)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.53)
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.55, ptr @str.54)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.56)
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.58, ptr @str.57)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.59)
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.61, ptr @str.60)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.62)
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

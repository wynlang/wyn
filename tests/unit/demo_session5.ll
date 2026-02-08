; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.1 = private unnamed_addr constant [46 x i8] c"\E2\95\91   WYN LANGUAGE - SESSION 5 FEATURES   \E2\95\91\00", align 1
@fmt.2 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.4 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.5 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.6 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.8 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.9 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.10 = private unnamed_addr constant [29 x i8] c"1. Random Number Generation:\00", align 1
@fmt.11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.12 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.13 = private unnamed_addr constant [12 x i8] c"   Random: \00", align 1
@fmt.14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.15 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@str.16 = private unnamed_addr constant [3 x i8] c", \00", align 1
@fmt.17 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.18 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.19 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.20 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.21 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.22 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.23 = private unnamed_addr constant [19 x i8] c"2. Sleep Function:\00", align 1
@fmt.24 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.25 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.26 = private unnamed_addr constant [20 x i8] c"   Sleeping 50ms...\00", align 1
@fmt.27 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.28 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.29 = private unnamed_addr constant [9 x i8] c"   Done!\00", align 1
@fmt.30 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.31 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.32 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.33 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.34 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.35 = private unnamed_addr constant [22 x i8] c"3. String Operations:\00", align 1
@fmt.36 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.37 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.38 = private unnamed_addr constant [16 x i8] c"Wyn is awesome!\00", align 1
@str.39 = private unnamed_addr constant [4 x i8] c"Wyn\00", align 1
@str.40 = private unnamed_addr constant [5 x i8] c"   '\00", align 1
@fmt.41 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.42 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@str.43 = private unnamed_addr constant [2 x i8] c"'\00", align 1
@fmt.44 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.45 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.46 = private unnamed_addr constant [20 x i8] c"   Contains 'Wyn': \00", align 1
@fmt.47 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.48 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.49 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.50 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.51 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.52 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.53 = private unnamed_addr constant [21 x i8] c"4. Boolean Literals:\00", align 1
@fmt.54 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.55 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.56 = private unnamed_addr constant [20 x i8] c"   \E2\9C\93 System ready\00", align 1
@fmt.57 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.58 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.59 = private unnamed_addr constant [24 x i8] c"   \E2\9C\97 Should not print\00", align 1
@fmt.60 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.61 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.62 = private unnamed_addr constant [22 x i8] c"   \E2\9C\93 System working\00", align 1
@fmt.63 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.64 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.65 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.66 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.67 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.68 = private unnamed_addr constant [19 x i8] c"5. Math Functions:\00", align 1
@fmt.69 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.70 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.71 = private unnamed_addr constant [18 x i8] c"   min(42, 17) = \00", align 1
@fmt.72 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.73 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.74 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.75 = private unnamed_addr constant [18 x i8] c"   max(42, 17) = \00", align 1
@fmt.76 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.77 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.78 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.79 = private unnamed_addr constant [15 x i8] c"   abs(-50) = \00", align 1
@fmt.80 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.81 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.82 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.83 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.84 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.85 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.86 = private unnamed_addr constant [22 x i8] c"6. Combined Features:\00", align 1
@fmt.87 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.88 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.89 = private unnamed_addr constant [21 x i8] c"   Positive random: \00", align 1
@fmt.90 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@fmt.91 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@nl.92 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.93 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@fmt.94 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.95 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.96 = private unnamed_addr constant [127 x i8] c"\E2\95\94\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\97\00", align 1
@fmt.97 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.98 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.99 = private unnamed_addr constant [48 x i8] c"\E2\95\91     ALL FEATURES WORKING! \E2\9C\93           \E2\95\91\00", align 1
@fmt.100 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.101 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@str.102 = private unnamed_addr constant [127 x i8] c"\E2\95\9A\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\90\E2\95\9D\00", align 1
@fmt.103 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@nl.104 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %is_positive = alloca i1, align 1
  %random_val = alloca i32, align 4
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %is_broken = alloca i1, align 1
  %is_ready = alloca i1, align 1
  %has_wyn = alloca i32, align 4
  %text = alloca ptr, align 8
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
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
  %rand = call i32 @rand()
  store i32 %rand, ptr %r1, align 4
  %rand1 = call i32 @rand()
  store i32 %rand1, ptr %r2, align 4
  %10 = call i32 (ptr, ...) @printf(ptr @fmt.14, ptr @str.13)
  %r12 = load i32, ptr %r1, align 4
  %11 = call i32 (ptr, ...) @printf(ptr @fmt.15, i32 %r12)
  %12 = call i32 (ptr, ...) @printf(ptr @fmt.17, ptr @str.16)
  %r23 = load i32, ptr %r2, align 4
  %13 = call i32 (ptr, ...) @printf(ptr @fmt.18, i32 %r23)
  %14 = call i32 (ptr, ...) @printf(ptr @nl.19)
  %15 = call i32 (ptr, ...) @printf(ptr @fmt.21, ptr @str.20)
  %16 = call i32 (ptr, ...) @printf(ptr @nl.22)
  %17 = call i32 (ptr, ...) @printf(ptr @fmt.24, ptr @str.23)
  %18 = call i32 (ptr, ...) @printf(ptr @nl.25)
  %19 = call i32 (ptr, ...) @printf(ptr @fmt.27, ptr @str.26)
  %20 = call i32 (ptr, ...) @printf(ptr @nl.28)
  %21 = call i32 @usleep(i32 50000)
  %22 = call i32 (ptr, ...) @printf(ptr @fmt.30, ptr @str.29)
  %23 = call i32 (ptr, ...) @printf(ptr @nl.31)
  %24 = call i32 (ptr, ...) @printf(ptr @fmt.33, ptr @str.32)
  %25 = call i32 (ptr, ...) @printf(ptr @nl.34)
  %26 = call i32 (ptr, ...) @printf(ptr @fmt.36, ptr @str.35)
  %27 = call i32 (ptr, ...) @printf(ptr @nl.37)
  store ptr @str.38, ptr %text, align 8
  %text4 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text4, ptr @str.39)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_wyn, align 4
  %28 = call i32 (ptr, ...) @printf(ptr @fmt.41, ptr @str.40)
  %text5 = load ptr, ptr %text, align 8
  %29 = call i32 (ptr, ...) @printf(ptr @fmt.42, ptr %text5)
  %30 = call i32 (ptr, ...) @printf(ptr @fmt.44, ptr @str.43)
  %31 = call i32 (ptr, ...) @printf(ptr @nl.45)
  %32 = call i32 (ptr, ...) @printf(ptr @fmt.47, ptr @str.46)
  %has_wyn6 = load i32, ptr %has_wyn, align 4
  %33 = call i32 (ptr, ...) @printf(ptr @fmt.48, i32 %has_wyn6)
  %34 = call i32 (ptr, ...) @printf(ptr @nl.49)
  %35 = call i32 (ptr, ...) @printf(ptr @fmt.51, ptr @str.50)
  %36 = call i32 (ptr, ...) @printf(ptr @nl.52)
  %37 = call i32 (ptr, ...) @printf(ptr @fmt.54, ptr @str.53)
  %38 = call i32 (ptr, ...) @printf(ptr @nl.55)
  store i1 true, ptr %is_ready, align 1
  store i1 false, ptr %is_broken, align 1
  %is_ready7 = load i1, ptr %is_ready, align 1
  br i1 %is_ready7, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %39 = call i32 (ptr, ...) @printf(ptr @fmt.57, ptr @str.56)
  %40 = call i32 (ptr, ...) @printf(ptr @nl.58)
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  %is_broken8 = load i1, ptr %is_broken, align 1
  br i1 %is_broken8, label %if.then9, label %if.else

if.then9:                                         ; preds = %if.end
  %41 = call i32 (ptr, ...) @printf(ptr @fmt.60, ptr @str.59)
  %42 = call i32 (ptr, ...) @printf(ptr @nl.61)
  br label %if.end10

if.else:                                          ; preds = %if.end
  %43 = call i32 (ptr, ...) @printf(ptr @fmt.63, ptr @str.62)
  %44 = call i32 (ptr, ...) @printf(ptr @nl.64)
  br label %if.end10

if.end10:                                         ; preds = %if.else, %if.then9
  %45 = call i32 (ptr, ...) @printf(ptr @fmt.66, ptr @str.65)
  %46 = call i32 (ptr, ...) @printf(ptr @nl.67)
  %47 = call i32 (ptr, ...) @printf(ptr @fmt.69, ptr @str.68)
  %48 = call i32 (ptr, ...) @printf(ptr @nl.70)
  store i32 42, ptr %x, align 4
  store i32 17, ptr %y, align 4
  %49 = call i32 (ptr, ...) @printf(ptr @fmt.72, ptr @str.71)
  %x11 = load i32, ptr %x, align 4
  %y12 = load i32, ptr %y, align 4
  %wyn_min = call i32 @wyn_min(i32 %x11, i32 %y12)
  %50 = call i32 (ptr, ...) @printf(ptr @fmt.73, i32 %wyn_min)
  %51 = call i32 (ptr, ...) @printf(ptr @nl.74)
  %52 = call i32 (ptr, ...) @printf(ptr @fmt.76, ptr @str.75)
  %x13 = load i32, ptr %x, align 4
  %y14 = load i32, ptr %y, align 4
  %wyn_max = call i32 @wyn_max(i32 %x13, i32 %y14)
  %53 = call i32 (ptr, ...) @printf(ptr @fmt.77, i32 %wyn_max)
  %54 = call i32 (ptr, ...) @printf(ptr @nl.78)
  %55 = call i32 (ptr, ...) @printf(ptr @fmt.80, ptr @str.79)
  %wyn_abs = call i32 @wyn_abs(i32 -50)
  %56 = call i32 (ptr, ...) @printf(ptr @fmt.81, i32 %wyn_abs)
  %57 = call i32 (ptr, ...) @printf(ptr @nl.82)
  %58 = call i32 (ptr, ...) @printf(ptr @fmt.84, ptr @str.83)
  %59 = call i32 (ptr, ...) @printf(ptr @nl.85)
  %60 = call i32 (ptr, ...) @printf(ptr @fmt.87, ptr @str.86)
  %61 = call i32 (ptr, ...) @printf(ptr @nl.88)
  %rand15 = call i32 @rand()
  store i32 %rand15, ptr %random_val, align 4
  store i1 true, ptr %is_positive, align 1
  %is_positive16 = load i1, ptr %is_positive, align 1
  br i1 %is_positive16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end10
  %random_val19 = load i32, ptr %random_val, align 4
  %wyn_max20 = call i32 @wyn_max(i32 %random_val19, i32 0)
  store i32 %wyn_max20, ptr %result, align 4
  %62 = call i32 (ptr, ...) @printf(ptr @fmt.90, ptr @str.89)
  %result21 = load i32, ptr %result, align 4
  %63 = call i32 (ptr, ...) @printf(ptr @fmt.91, i32 %result21)
  %64 = call i32 (ptr, ...) @printf(ptr @nl.92)
  br label %if.end18

if.end18:                                         ; preds = %if.then17, %if.end10
  %65 = call i32 (ptr, ...) @printf(ptr @fmt.94, ptr @str.93)
  %66 = call i32 (ptr, ...) @printf(ptr @nl.95)
  %67 = call i32 (ptr, ...) @printf(ptr @fmt.97, ptr @str.96)
  %68 = call i32 (ptr, ...) @printf(ptr @nl.98)
  %69 = call i32 (ptr, ...) @printf(ptr @fmt.100, ptr @str.99)
  %70 = call i32 (ptr, ...) @printf(ptr @nl.101)
  %71 = call i32 (ptr, ...) @printf(ptr @fmt.103, ptr @str.102)
  %72 = call i32 (ptr, ...) @printf(ptr @nl.104)
  ret i32 0
}

declare i32 @rand()

declare i32 @usleep(i32)

declare ptr @strstr(ptr, ptr)

declare i32 @wyn_min(i32, i32)

declare i32 @wyn_max(i32, i32)

declare i32 @wyn_abs(i32)

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %sign3 = alloca i32, align 4
  %sign2 = alloca i32, align 4
  %sign1 = alloca i32, align 4
  %c2 = alloca i32, align 4
  %c1 = alloca i32, align 4
  %prime2 = alloca i32, align 4
  %prime1 = alloca i32, align 4
  %f1 = alloca i32, align 4
  %l1 = alloca i32, align 4
  %g1 = alloca i32, align 4
  %s1 = alloca i32, align 4
  %p1 = alloca i32, align 4
  %p11 = load i32, ptr %p1, align 4
  %icmp = icmp ne i32 %p11, 32
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %s12 = load i32, ptr %s1, align 4
  %icmp3 = icmp ne i32 %s12, 4
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %g16 = load i32, ptr %g1, align 4
  %icmp7 = icmp ne i32 %g16, 6
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end5
  ret i32 3

if.end9:                                          ; preds = %if.end5
  %l110 = load i32, ptr %l1, align 4
  %icmp11 = icmp ne i32 %l110, 12
  br i1 %icmp11, label %if.then12, label %if.end13

if.then12:                                        ; preds = %if.end9
  ret i32 4

if.end13:                                         ; preds = %if.end9
  %f114 = load i32, ptr %f1, align 4
  %icmp15 = icmp ne i32 %f114, 120
  br i1 %icmp15, label %if.then16, label %if.end17

if.then16:                                        ; preds = %if.end13
  ret i32 5

if.end17:                                         ; preds = %if.end13
  %prime118 = load i32, ptr %prime1, align 4
  %icmp19 = icmp ne i32 %prime118, 1
  br i1 %icmp19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end17
  ret i32 6

if.end21:                                         ; preds = %if.end17
  %prime222 = load i32, ptr %prime2, align 4
  %icmp23 = icmp ne i32 %prime222, 0
  br i1 %icmp23, label %if.then24, label %if.end25

if.then24:                                        ; preds = %if.end21
  ret i32 7

if.end25:                                         ; preds = %if.end21
  %c126 = load i32, ptr %c1, align 4
  %icmp27 = icmp ne i32 %c126, 10
  br i1 %icmp27, label %if.then28, label %if.end29

if.then28:                                        ; preds = %if.end25
  ret i32 8

if.end29:                                         ; preds = %if.end25
  %c230 = load i32, ptr %c2, align 4
  %icmp31 = icmp ne i32 %c230, 5
  br i1 %icmp31, label %if.then32, label %if.end33

if.then32:                                        ; preds = %if.end29
  ret i32 9

if.end33:                                         ; preds = %if.end29
  %sign134 = load i32, ptr %sign1, align 4
  %icmp35 = icmp ne i32 %sign134, -1
  br i1 %icmp35, label %if.then36, label %if.end37

if.then36:                                        ; preds = %if.end33
  ret i32 10

if.end37:                                         ; preds = %if.end33
  %sign238 = load i32, ptr %sign2, align 4
  %icmp39 = icmp ne i32 %sign238, 1
  br i1 %icmp39, label %if.then40, label %if.end41

if.then40:                                        ; preds = %if.end37
  ret i32 11

if.end41:                                         ; preds = %if.end37
  %sign342 = load i32, ptr %sign3, align 4
  %icmp43 = icmp ne i32 %sign342, 0
  br i1 %icmp43, label %if.then44, label %if.end45

if.then44:                                        ; preds = %if.end41
  ret i32 12

if.end45:                                         ; preds = %if.end41
  ret i32 0
}

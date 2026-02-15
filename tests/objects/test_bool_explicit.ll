; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %to_int_f = alloca i32, align 4
  %to_int_t = alloca i32, align 4
  %xor_result = alloca i32, align 4
  %or_result = alloca i32, align 4
  %and_result = alloca i32, align 4
  %not_t = alloca i32, align 4
  %f = alloca i1, align 1
  %t = alloca i1, align 1
  store i1 true, ptr %t, align 1
  store i1 false, ptr %f, align 1
  %t1 = load i1, ptr %t, align 1
  %not_t2 = load i32, ptr %not_t, align 4
  %icmp = icmp ne i32 %not_t2, i1 false
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %t3 = load i1, ptr %t, align 1
  %and_result4 = load i32, ptr %and_result, align 4
  %icmp5 = icmp ne i32 %and_result4, i1 false
  br i1 %icmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  ret i32 2

if.end7:                                          ; preds = %if.end
  %t8 = load i1, ptr %t, align 1
  %or_result9 = load i32, ptr %or_result, align 4
  %icmp10 = icmp ne i32 %or_result9, i1 true
  br i1 %icmp10, label %if.then11, label %if.end12

if.then11:                                        ; preds = %if.end7
  ret i32 3

if.end12:                                         ; preds = %if.end7
  %t13 = load i1, ptr %t, align 1
  %xor_result14 = load i32, ptr %xor_result, align 4
  %icmp15 = icmp ne i32 %xor_result14, i1 true
  br i1 %icmp15, label %if.then16, label %if.end17

if.then16:                                        ; preds = %if.end12
  ret i32 4

if.end17:                                         ; preds = %if.end12
  %t18 = load i1, ptr %t, align 1
  %to_int_t19 = load i32, ptr %to_int_t, align 4
  %icmp20 = icmp ne i32 %to_int_t19, 1
  br i1 %icmp20, label %if.then21, label %if.end22

if.then21:                                        ; preds = %if.end17
  ret i32 5

if.end22:                                         ; preds = %if.end17
  %f23 = load i1, ptr %f, align 1
  %to_int_f24 = load i32, ptr %to_int_f, align 4
  %icmp25 = icmp ne i32 %to_int_f24, 0
  br i1 %icmp25, label %if.then26, label %if.end27

if.then26:                                        ; preds = %if.end22
  ret i32 6

if.end27:                                         ; preds = %if.end22
  ret i32 0
}

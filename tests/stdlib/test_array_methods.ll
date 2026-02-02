; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %l = alloca i32, align 4
  %f = alloca i32, align 4
  %last = alloca i32, align 4
  %arr = alloca ptr, align 8
  %array_literal = alloca [3 x i32], align 4
  %element_ptr = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr [3 x i32], ptr %array_literal, i32 0, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %array_literal, ptr %arr, align 8
  %arr3 = load ptr, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %last5 = load i32, ptr %last, align 4
  %icmp = icmp ne i32 %last5, 4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 2

if.end:                                           ; preds = %entry
  %arr6 = load ptr, ptr %arr, align 8
  %f7 = load i32, ptr %f, align 4
  %icmp8 = icmp ne i32 %f7, 1
  br i1 %icmp8, label %if.then9, label %if.end10

if.then9:                                         ; preds = %if.end
  ret i32 3

if.end10:                                         ; preds = %if.end
  %arr11 = load ptr, ptr %arr, align 8
  %l12 = load i32, ptr %l, align 4
  %icmp13 = icmp ne i32 %l12, 3
  br i1 %icmp13, label %if.then14, label %if.end15

if.then14:                                        ; preds = %if.end10
  ret i32 4

if.end15:                                         ; preds = %if.end10
  %arr16 = load ptr, ptr %arr, align 8
  %not = xor ptr %arr16, <13 x bfloat> splat (bfloat 0xRFFFF)
  %strstr_result = call ptr @strstr(ptr %not, i32 2)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  %tobool = icmp ne i32 %contains, 0
  br i1 %tobool, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end15
  ret i32 5

if.end18:                                         ; preds = %if.end15
  %arr19 = load ptr, ptr %arr, align 8
  %strstr_result20 = call ptr @strstr(ptr %arr19, i32 99)
  %is_found21 = icmp ne ptr %strstr_result20, null
  %contains22 = zext i1 %is_found21 to i32
  %tobool23 = icmp ne i32 %contains22, 0
  br i1 %tobool23, label %if.then24, label %if.end25

if.then24:                                        ; preds = %if.end18
  ret i32 6

if.end25:                                         ; preds = %if.end18
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

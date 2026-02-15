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
  %data_size = mul i64 3, ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64)
  %total_size = add i64 8, %data_size
  %array_alloc = call ptr @malloc(i64 %total_size)
  store i64 3, ptr %array_alloc, align 4
  %data_ptr = getelementptr i8, ptr %array_alloc, i64 8
  %element_ptr = getelementptr i32, ptr %data_ptr, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr i32, ptr %data_ptr, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr i32, ptr %data_ptr, i32 2
  store i32 3, ptr %element_ptr2, align 4
  store ptr %data_ptr, ptr %arr, align 8
  %arr3 = load ptr, ptr %arr, align 8
  %arr4 = load ptr, ptr %arr, align 8
  %last5 = load i32, ptr %last, align 4
  %icmp = icmp ne i32 %last5, 4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 2

if.end:                                           ; preds = %entry
  %arr6 = load ptr, ptr %arr, align 8
  %first_ptr = getelementptr i32, ptr %arr6, i32 0
  %first = load i32, ptr %first_ptr, align 4
  store i32 %first, ptr %f, align 4
  %f7 = load i32, ptr %f, align 4
  %icmp8 = icmp ne i32 %f7, 1
  br i1 %icmp8, label %if.then9, label %if.end10

if.then9:                                         ; preds = %if.end
  ret i32 3

if.end10:                                         ; preds = %if.end
  %arr11 = load ptr, ptr %arr, align 8
  %length_i8_ptr = getelementptr i8, ptr %arr11, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %last_index = sub i32 %length_int, 1
  %last_ptr = getelementptr i32, ptr %arr11, i32 %last_index
  %last12 = load i32, ptr %last_ptr, align 4
  store i32 %last12, ptr %l, align 4
  %l13 = load i32, ptr %l, align 4
  %icmp14 = icmp ne i32 %l13, 3
  br i1 %icmp14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end10
  ret i32 4

if.end16:                                         ; preds = %if.end10
  %arr17 = load ptr, ptr %arr, align 8
  %not = xor ptr %arr17, <13 x bfloat> splat (bfloat 0xRFFFF)
  %strstr_result = call ptr @strstr(ptr %not, i32 2)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  %tobool = icmp ne i32 %contains, 0
  br i1 %tobool, label %if.then18, label %if.end19

if.then18:                                        ; preds = %if.end16
  ret i32 5

if.end19:                                         ; preds = %if.end16
  %arr20 = load ptr, ptr %arr, align 8
  %strstr_result21 = call ptr @strstr(ptr %arr20, i32 99)
  %is_found22 = icmp ne ptr %strstr_result21, null
  %contains23 = zext i1 %is_found22 to i32
  %tobool24 = icmp ne i32 %contains23, 0
  br i1 %tobool24, label %if.then25, label %if.end26

if.then25:                                        ; preds = %if.end19
  ret i32 6

if.end26:                                         ; preds = %if.end19
  ret i32 0
}

declare ptr @strstr(ptr, ptr)

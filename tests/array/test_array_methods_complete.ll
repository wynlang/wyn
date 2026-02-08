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
  %arr = alloca ptr, align 8
  %data_size = mul i64 5, ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64)
  %total_size = add i64 8, %data_size
  %array_alloc = call ptr @malloc(i64 %total_size)
  store i64 5, ptr %array_alloc, align 4
  %data_ptr = getelementptr i8, ptr %array_alloc, i64 8
  %element_ptr = getelementptr i32, ptr %data_ptr, i32 0
  store i32 10, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr i32, ptr %data_ptr, i32 1
  store i32 20, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr i32, ptr %data_ptr, i32 2
  store i32 30, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr i32, ptr %data_ptr, i32 3
  store i32 40, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr i32, ptr %data_ptr, i32 4
  store i32 50, ptr %element_ptr4, align 4
  store ptr %data_ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %length_i8_ptr = getelementptr i8, ptr %arr5, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %icmp = icmp ne i32 %length_int, 5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %arr6 = load ptr, ptr %arr, align 8
  %first_ptr = getelementptr i32, ptr %arr6, i32 0
  %first = load i32, ptr %first_ptr, align 4
  store i32 %first, ptr %f, align 4
  %f7 = load i32, ptr %f, align 4
  %icmp8 = icmp ne i32 %f7, 10
  br i1 %icmp8, label %if.then9, label %if.end10

if.then9:                                         ; preds = %if.end
  ret i32 2

if.end10:                                         ; preds = %if.end
  %arr11 = load ptr, ptr %arr, align 8
  %length_i8_ptr12 = getelementptr i8, ptr %arr11, i64 -8
  %array_length13 = load i64, ptr %length_i8_ptr12, align 4
  %length_int14 = trunc i64 %array_length13 to i32
  %last_index = sub i32 %length_int14, 1
  %last_ptr = getelementptr i32, ptr %arr11, i32 %last_index
  %last = load i32, ptr %last_ptr, align 4
  store i32 %last, ptr %l, align 4
  %l15 = load i32, ptr %l, align 4
  %icmp16 = icmp ne i32 %l15, 50
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end10
  ret i32 3

if.end18:                                         ; preds = %if.end10
  ret i32 0
}

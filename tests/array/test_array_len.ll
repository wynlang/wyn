; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %len2 = alloca i32, align 4
  %arr2 = alloca ptr, align 8
  %len = alloca i32, align 4
  %arr = alloca ptr, align 8
  %data_size = mul i64 5, ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64)
  %total_size = add i64 8, %data_size
  %array_alloc = call ptr @malloc(i64 %total_size)
  store i64 5, ptr %array_alloc, align 4
  %data_ptr = getelementptr i8, ptr %array_alloc, i64 8
  %element_ptr = getelementptr i32, ptr %data_ptr, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr i32, ptr %data_ptr, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr i32, ptr %data_ptr, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr i32, ptr %data_ptr, i32 3
  store i32 4, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr i32, ptr %data_ptr, i32 4
  store i32 5, ptr %element_ptr4, align 4
  store ptr %data_ptr, ptr %arr, align 8
  %arr5 = load ptr, ptr %arr, align 8
  %length_i8_ptr = getelementptr i8, ptr %arr5, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  store i32 %length_int, ptr %len, align 4
  %len6 = load i32, ptr %len, align 4
  %icmp = icmp ne i32 %len6, 5
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %data_size7 = mul i64 2, ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64)
  %total_size8 = add i64 8, %data_size7
  %array_alloc9 = call ptr @malloc(i64 %total_size8)
  store i64 2, ptr %array_alloc9, align 4
  %data_ptr10 = getelementptr i8, ptr %array_alloc9, i64 8
  %element_ptr11 = getelementptr i32, ptr %data_ptr10, i32 0
  store i32 10, ptr %element_ptr11, align 4
  %element_ptr12 = getelementptr i32, ptr %data_ptr10, i32 1
  store i32 20, ptr %element_ptr12, align 4
  store ptr %data_ptr10, ptr %arr2, align 8
  %arr213 = load ptr, ptr %arr2, align 8
  %length_i8_ptr14 = getelementptr i8, ptr %arr213, i64 -8
  %array_length15 = load i64, ptr %length_i8_ptr14, align 4
  %length_int16 = trunc i64 %array_length15 to i32
  store i32 %length_int16, ptr %len2, align 4
  %len217 = load i32, ptr %len2, align 4
  %icmp18 = icmp ne i32 %len217, 2
  br i1 %icmp18, label %if.then19, label %if.end20

if.then19:                                        ; preds = %if.end
  ret i32 2

if.end20:                                         ; preds = %if.end
  ret i32 0
}

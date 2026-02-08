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
  %first_ptr = getelementptr i32, ptr %arr3, i32 0
  %first = load i32, ptr %first_ptr, align 4
  store i32 %first, ptr %f, align 4
  %f4 = load i32, ptr %f, align 4
  %icmp = icmp ne i32 %f4, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %arr5 = load ptr, ptr %arr, align 8
  %length_i8_ptr = getelementptr i8, ptr %arr5, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %last_index = sub i32 %length_int, 1
  %last_ptr = getelementptr i32, ptr %arr5, i32 %last_index
  %last = load i32, ptr %last_ptr, align 4
  store i32 %last, ptr %l, align 4
  %l6 = load i32, ptr %l, align 4
  %icmp7 = icmp ne i32 %l6, 3
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end
  ret i32 2

if.end9:                                          ; preds = %if.end
  ret i32 0
}

; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %empty = alloca ptr, align 8
  %last16 = alloca i32, align 4
  %first10 = alloca i32, align 4
  %count = alloca i32, align 4
  %numbers = alloca ptr, align 8
  %data_size = mul i64 7, ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64)
  %total_size = add i64 8, %data_size
  %array_alloc = call ptr @malloc(i64 %total_size)
  store i64 7, ptr %array_alloc, align 4
  %data_ptr = getelementptr i8, ptr %array_alloc, i64 8
  %element_ptr = getelementptr i32, ptr %data_ptr, i32 0
  store i32 1, ptr %element_ptr, align 4
  %element_ptr1 = getelementptr i32, ptr %data_ptr, i32 1
  store i32 2, ptr %element_ptr1, align 4
  %element_ptr2 = getelementptr i32, ptr %data_ptr, i32 2
  store i32 3, ptr %element_ptr2, align 4
  %element_ptr3 = getelementptr i32, ptr %data_ptr, i32 3
  store i32 2, ptr %element_ptr3, align 4
  %element_ptr4 = getelementptr i32, ptr %data_ptr, i32 4
  store i32 4, ptr %element_ptr4, align 4
  %element_ptr5 = getelementptr i32, ptr %data_ptr, i32 5
  store i32 2, ptr %element_ptr5, align 4
  %element_ptr6 = getelementptr i32, ptr %data_ptr, i32 6
  store i32 5, ptr %element_ptr6, align 4
  store ptr %data_ptr, ptr %numbers, align 8
  %numbers7 = load ptr, ptr %numbers, align 8
  %count8 = load i32, ptr %count, align 4
  %icmp = icmp ne i32 %count8, 3
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %numbers9 = load ptr, ptr %numbers, align 8
  %first_ptr = getelementptr i32, ptr %numbers9, i32 0
  %first = load i32, ptr %first_ptr, align 4
  store i32 %first, ptr %first10, align 4
  %first11 = load i32, ptr %first10, align 4
  %icmp12 = icmp ne i32 %first11, 1
  br i1 %icmp12, label %if.then13, label %if.end14

if.then13:                                        ; preds = %if.end
  ret i32 2

if.end14:                                         ; preds = %if.end
  %numbers15 = load ptr, ptr %numbers, align 8
  %length_i8_ptr = getelementptr i8, ptr %numbers15, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %last_index = sub i32 %length_int, 1
  %last_ptr = getelementptr i32, ptr %numbers15, i32 %last_index
  %last = load i32, ptr %last_ptr, align 4
  store i32 %last, ptr %last16, align 4
  %last17 = load i32, ptr %last16, align 4
  %icmp18 = icmp ne i32 %last17, 5
  br i1 %icmp18, label %if.then19, label %if.end20

if.then19:                                        ; preds = %if.end14
  ret i32 3

if.end20:                                         ; preds = %if.end14
  %array_alloc21 = call ptr @malloc(i64 8)
  store i64 0, ptr %array_alloc21, align 4
  %data_ptr22 = getelementptr i8, ptr %array_alloc21, i64 8
  store ptr %data_ptr22, ptr %empty, align 8
  %empty23 = load ptr, ptr %empty, align 8
  %numbers24 = load ptr, ptr %numbers, align 8
  ret i32 0
}

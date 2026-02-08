; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %values = alloca i32, align 4
  %keys = alloca i32, align 4
  %map = alloca i32, align 4
  %map1 = load i32, ptr %map, align 4
  %keys2 = load i32, ptr %keys, align 4
  %i8_ptr = bitcast i32 %keys2 to ptr
  %length_i8_ptr = getelementptr i8, ptr %i8_ptr, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %icmp = icmp ne i32 %length_int, 3
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %map3 = load i32, ptr %map, align 4
  %values4 = load i32, ptr %values, align 4
  %i8_ptr5 = bitcast i32 %values4 to ptr
  %length_i8_ptr6 = getelementptr i8, ptr %i8_ptr5, i64 -8
  %array_length7 = load i64, ptr %length_i8_ptr6, align 4
  %length_int8 = trunc i64 %array_length7 to i32
  %icmp9 = icmp ne i32 %length_int8, 3
  br i1 %icmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end
  ret i32 2

if.end11:                                         ; preds = %if.end
  %map12 = load i32, ptr %map, align 4
  %map13 = load i32, ptr %map, align 4
  %strlen = call i64 @strlen(i32 %map13)
  %icmp14 = icmp ne i64 %strlen, i32 0
  br i1 %icmp14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end11
  ret i32 3

if.end16:                                         ; preds = %if.end11
  ret i32 0
}

declare i64 @strlen(ptr)

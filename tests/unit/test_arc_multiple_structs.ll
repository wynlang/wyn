; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Data = type { i32, i32, i32 }
%Box = type { i32 }
%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %d = alloca %Data, align 8
  %b = alloca %Box, align 8
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  store %Box { i32 5 }, ptr %b, align 4
  store %Data { i32 1, i32 2, i32 3 }, ptr %d, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 0
  %p2 = load %Point, ptr %p, align 4
  %field_val3 = extractvalue %Point %p2, 1
  %add = add i32 %field_val, %field_val3
  %b4 = load %Box, ptr %b, align 4
  %field_val5 = extractvalue %Box %b4, 0
  %add6 = add i32 %add, %field_val5
  %d7 = load %Data, ptr %d, align 4
  %field_val8 = extractvalue %Data %d7, 0
  %add9 = add i32 %add6, %field_val8
  %d10 = load %Data, ptr %d, align 4
  %field_val11 = extractvalue %Data %d10, 1
  %add12 = add i32 %add9, %field_val11
  %d13 = load %Data, ptr %d, align 4
  %field_val14 = extractvalue %Data %d13, 2
  %add15 = add i32 %add12, %field_val14
  ret i32 %add15
}

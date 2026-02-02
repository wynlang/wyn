; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Item.0 = type { i32, i32 }
%Item = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@RED = internal constant i32 0
@GREEN = internal constant i32 1
@BLUE = internal constant i32 2

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare i32 @wyn_system_argc()

declare ptr @wyn_system_argv(i32)

declare ptr @wyn_system_env(ptr)

declare i32 @wyn_system_set_env(ptr, ptr)

declare i32 @wyn_system_exec(ptr)

declare ptr @wyn_file_read(ptr)

declare i32 @wyn_file_write(ptr, ptr)

declare i32 @wyn_file_exists(ptr)

declare i32 @wyn_file_is_file(ptr)

declare i32 @wyn_file_is_dir(ptr)

declare ptr @wyn_file_join(ptr, ptr)

declare ptr @wyn_file_basename(ptr)

declare ptr @wyn_file_dirname(ptr)

declare ptr @wyn_file_extension(ptr)

declare ptr @wyn_file_cwd()

declare i64 @wyn_time_now()

declare void @wyn_time_sleep(i64)

define i32 @wyn_main() {
entry:
  %item211 = alloca %Item.0, align 8
  %item = alloca %Item, align 8
  %RED = load i32, ptr @RED, align 4
  %struct_val = insertvalue %Item undef, i32 %RED, 0
  %struct_val1 = insertvalue %Item %struct_val, i32 42, 1
  store %Item %struct_val1, ptr %item, align 4
  %item2 = load %Item, ptr %item, align 4
  %field_val = extractvalue %Item %item2, 1
  %icmp = icmp ne i32 %field_val, 42
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %item3 = load %Item, ptr %item, align 4
  %field_val4 = extractvalue %Item %item3, 0
  %RED5 = load i32, ptr @RED, align 4
  %icmp6 = icmp ne i32 %field_val4, %RED5
  br i1 %icmp6, label %if.then7, label %if.end8

if.then7:                                         ; preds = %if.end
  ret i32 2

if.end8:                                          ; preds = %if.end
  %GREEN = load i32, ptr @GREEN, align 4
  %struct_val9 = insertvalue %Item.0 undef, i32 %GREEN, 0
  %struct_val10 = insertvalue %Item.0 %struct_val9, i32 10, 1
  store %Item.0 %struct_val10, ptr %item211, align 4
  %item12 = load %Item, ptr %item, align 4
  %field_val13 = extractvalue %Item %item12, 0
  %item214 = load %Item.0, ptr %item211, align 4
  ret i32 0
}

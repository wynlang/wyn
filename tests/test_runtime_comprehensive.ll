; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [5 x i8] c"PATH\00", align 1

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
  %cwd = alloca ptr, align 8
  %t2 = alloca i64, align 8
  %t1 = alloca i64, align 8
  %path = alloca ptr, align 8
  %"System::env" = call ptr @wyn_system_env(ptr @str)
  store ptr %"System::env", ptr %path, align 8
  %"Time::now" = call i64 @wyn_time_now()
  store i64 %"Time::now", ptr %t1, align 4
  %t11 = load i64, ptr %t1, align 4
  %icmp = icmp sle i64 %t11, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %"Time::sleep" = call void @wyn_time_sleep(i32 1)
  %"Time::now2" = call i64 @wyn_time_now()
  store i64 %"Time::now2", ptr %t2, align 4
  %t23 = load i64, ptr %t2, align 4
  %t14 = load i64, ptr %t1, align 4
  %icmp5 = icmp sle i64 %t23, %t14
  br i1 %icmp5, label %if.then6, label %if.end7

if.then6:                                         ; preds = %if.end
  ret i32 2

if.end7:                                          ; preds = %if.end
  %"File::cwd" = call ptr @wyn_file_cwd()
  store ptr %"File::cwd", ptr %cwd, align 8
  ret i32 0
}

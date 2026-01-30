; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @next_state(i32 %current) {
entry:
  %current1 = alloca i32, align 4
  store i32 %current, ptr %current1, align 4
  ret i32 0
}

define i32 @state_to_int(i32 %s) {
entry:
  %s1 = alloca i32, align 4
  store i32 %s, ptr %s1, align 4
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %s4 = alloca i32, align 4
  %s3 = alloca i32, align 4
  %s2 = alloca i32, align 4
  %s1 = alloca i32, align 4
  %s11 = load i32, ptr %s1, align 4
  %next_state = call i32 @next_state(i32 %s11)
  store i32 %next_state, ptr %s2, align 4
  %s22 = load i32, ptr %s2, align 4
  %next_state3 = call i32 @next_state(i32 %s22)
  store i32 %next_state3, ptr %s3, align 4
  %s34 = load i32, ptr %s3, align 4
  %next_state5 = call i32 @next_state(i32 %s34)
  store i32 %next_state5, ptr %s4, align 4
  %s26 = load i32, ptr %s2, align 4
  %state_to_int = call i32 @state_to_int(i32 %s26)
  %s37 = load i32, ptr %s3, align 4
  %state_to_int8 = call i32 @state_to_int(i32 %s37)
  %add = add i32 %state_to_int, %state_to_int8
  %s49 = load i32, ptr %s4, align 4
  %state_to_int10 = call i32 @state_to_int(i32 %s49)
  %add11 = add i32 %add, %state_to_int10
  ret i32 %add11
}

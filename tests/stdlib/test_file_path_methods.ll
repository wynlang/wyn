; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %cwd = alloca i32, align 4
  %ext = alloca i32, align 4
  %dir = alloca i32, align 4
  %base = alloca i32, align 4
  %path1 = alloca i32, align 4
  %path11 = load i32, ptr %path1, align 4
  %strlen = call i64 @strlen(i32 %path11)
  %icmp = icmp eq i64 %strlen, i32 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %base2 = load i32, ptr %base, align 4
  %strlen3 = call i64 @strlen(i32 %base2)
  %icmp4 = icmp eq i64 %strlen3, i32 0
  br i1 %icmp4, label %if.then5, label %if.end6

if.then5:                                         ; preds = %if.end
  ret i32 2

if.end6:                                          ; preds = %if.end
  %dir7 = load i32, ptr %dir, align 4
  %strlen8 = call i64 @strlen(i32 %dir7)
  %icmp9 = icmp eq i64 %strlen8, i32 0
  br i1 %icmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end6
  ret i32 3

if.end11:                                         ; preds = %if.end6
  %ext12 = load i32, ptr %ext, align 4
  %strlen13 = call i64 @strlen(i32 %ext12)
  %icmp14 = icmp eq i64 %strlen13, i32 0
  br i1 %icmp14, label %if.then15, label %if.end16

if.then15:                                        ; preds = %if.end11
  ret i32 4

if.end16:                                         ; preds = %if.end11
  %cwd17 = load i32, ptr %cwd, align 4
  %strlen18 = call i64 @strlen(i32 %cwd17)
  %icmp19 = icmp eq i64 %strlen18, i32 0
  br i1 %icmp19, label %if.then20, label %if.end21

if.then20:                                        ; preds = %if.end16
  ret i32 5

if.end21:                                         ; preds = %if.end16
  ret i32 0
}

declare i64 @strlen(ptr)

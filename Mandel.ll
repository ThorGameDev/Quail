; ModuleID = 'QuailJIT'
source_filename = "QuailJIT"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

declare void @putchard(i8)

define void @printdensity(i32 %d) {
entry:
  %tgttmp = icmp sgt i32 %d, 8
  br i1 %tgttmp, label %then, label %else

then:                                             ; preds = %entry
  %calltmp = call void @putchard(i8 32)
  br label %ifcont21

else:                                             ; preds = %entry
  %tgttmp4 = icmp sgt i32 %d, 4
  br i1 %tgttmp4, label %then5, label %else9

then5:                                            ; preds = %else
  %calltmp7 = call void @putchard(i8 46)
  br label %ifcont21

else9:                                            ; preds = %else
  %tgttmp11 = icmp sgt i32 %d, 2
  br i1 %tgttmp11, label %then12, label %else16

then12:                                           ; preds = %else9
  %calltmp14 = call void @putchard(i8 43)
  br label %ifcont21

else16:                                           ; preds = %else9
  %calltmp18 = call void @putchard(i8 42)
  br label %ifcont21

ifcont21:                                         ; preds = %then5, %else16, %then12, %then
  ret void undef
}

define i32 @mandelconverger(float %real, float %imag, i32 %iters, float %creal, float %cimag) {
entry:
  %tgttmp = icmp sgt i32 %iters, 255
  %multmp = fmul float %real, %real
  %multmp11 = fmul float %imag, %imag
  %addtmp = fadd float %multmp, %multmp11
  %tgttmp12 = fcmp ugt float %addtmp, 4.000000e+00
  %xortmp = xor i1 %tgttmp, %tgttmp12
  br i1 %xortmp, label %block0end, label %ifcont

ifcont:                                           ; preds = %entry
  %subtmp = fsub float %multmp, %multmp11
  %addtmp21 = fadd float %subtmp, %creal
  %multmp23 = fmul float %real, 2.000000e+00
  %multmp25 = fmul float %multmp23, %imag
  %addtmp27 = fadd float %multmp25, %cimag
  %addtmp29 = add i32 %iters, 1
  %calltmp = call i32 @mandelconverger(float %addtmp21, float %addtmp27, i32 %addtmp29, float %creal, float %cimag)
  br label %block0end

block0end:                                        ; preds = %entry, %ifcont
  %retval = phi i32 [ %calltmp, %ifcont ], [ %iters, %entry ]
  ret i32 %retval
}

define i32 @mandelconverge(float %real, float %imag) {
entry:
  %calltmp = call i32 @mandelconverger(float %real, float %imag, i32 0, float %real, float %imag)
  ret i32 %calltmp
}

define void @mandelhelp(float %xmin, float %xmax, float %xstep, float %ymin, float %ymax, float %ystep) {
entry:
  br label %conditionblock21

loop9:                                            ; preds = %conditionblock
  %calltmp = call i32 @mandelconverge(float %x.0, float %y.0)
  %calltmp12 = call void @printdensity(i32 %calltmp)
  %addtmp = fadd float %xstep, %x.0
  br label %conditionblock

conditionblock:                                   ; preds = %conditionblock21, %loop9
  %x.0 = phi float [ %addtmp, %loop9 ], [ %xmin, %conditionblock21 ]
  %tlttmp = fcmp ult float %x.0, %xmax
  br i1 %tlttmp, label %loop9, label %afterloop

afterloop:                                        ; preds = %conditionblock
  %calltmp17 = call void @putchard(i8 10)
  %addtmp20 = fadd float %ystep, %y.0
  br label %conditionblock21

conditionblock21:                                 ; preds = %entry, %afterloop
  %y.0 = phi float [ %ymin, %entry ], [ %addtmp20, %afterloop ]
  %tlttmp24 = fcmp ult float %y.0, %ymax
  br i1 %tlttmp24, label %conditionblock, label %afterloop25

afterloop25:                                      ; preds = %conditionblock21
  ret void undef
}

define void @mandel(float %realstart, float %imagstart, float %realmag, float %imagmag) {
entry:
  %multmp = fmul float %realmag, 7.800000e+01
  %addtmp = fadd float %realstart, %multmp
  %multmp12 = fmul float %imagmag, 4.000000e+01
  %addtmp13 = fadd float %imagstart, %multmp12
  %calltmp = call void @mandelhelp(float %realstart, float %addtmp, float %realmag, float %imagstart, float %addtmp13, float %imagmag)
  ret void undef
}

define void @main() {
entry:
  %calltmp = call void @mandel(float 0xC002666660000000, float 0xBFF4CCCCC0000000, float 0x3FA99999A0000000, float 0x3FB1EB8520000000)
  %calltmp1 = call void @mandel(float -2.000000e+00, float -1.000000e+00, float 0x3F947AE140000000, float 0x3FA47AE140000000)
  %calltmp2 = call void @mandel(float 0xBFECCCCCC0000000, float 0xBFF6666660000000, float 0x3F947AE140000000, float 0x3F9EB851E0000000)
  ret void undef
}

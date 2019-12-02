DEFINES = /D_WIN32_WINNT=0x400 /DISOLATION_AWARE_ENABLED /D_WIN32_IE=0x500 /DWIN32_LEAN_AND_MEAN /DWIN32
CFLAGS  = /W3 /nologo -I..\common\win32 /O /D_DEBUG /Zi

HEADERS = ..\common\win32\freeze.h sketchflat.h ui.h sketch.h derived.h

OBJDIR = obj

FREEZE   = $(OBJDIR)\freeze.obj

SKOBJS   = $(OBJDIR)\sketchflat.obj \
           $(OBJDIR)\loadsave.obj \
           $(OBJDIR)\undoredo.obj \
           $(OBJDIR)\win32util.obj \
           $(OBJDIR)\win32window.obj \
           $(OBJDIR)\win32simple.obj \
           $(OBJDIR)\win32text.obj \
           $(OBJDIR)\util.obj \
           $(OBJDIR)\layer.obj \
           $(OBJDIR)\draw.obj \
           $(OBJDIR)\draw_sketch.obj \
           $(OBJDIR)\draw_constraint.obj \
           $(OBJDIR)\sketch.obj \
           $(OBJDIR)\measure.obj \
           $(OBJDIR)\curve.obj \
           $(OBJDIR)\polygon.obj \
           $(OBJDIR)\derive.obj \
           $(OBJDIR)\expr.obj \
           $(OBJDIR)\constraint.obj \
           $(OBJDIR)\solve.obj \
           $(OBJDIR)\assume.obj \
           $(OBJDIR)\newton.obj \
           $(OBJDIR)\ttf.obj \
           $(OBJDIR)\export.obj \

LIBS = user32.lib gdi32.lib comctl32.lib advapi32.lib

all: $(OBJDIR)/sketchflat.exe
    @cp $(OBJDIR)/sketchflat.exe .
    sketchflat asd.skf

clean:
	rm -f obj/*

$(OBJDIR)/sketchflat.exe: $(SKOBJS) $(FREEZE) $(OBJDIR)/sketchflat.res
    @$(CC) $(DEFINES) $(CFLAGS) -Fe$(OBJDIR)/sketchflat.exe $(SKOBJS) $(FREEZE) $(OBJDIR)/sketchflat.res $(LIBS)
    @echo sketchflat.exe

$(OBJDIR)/sketchflat.res: sketchflat.rc sketchflat.ico
	@rc sketchflat.rc
	@mv sketchflat.res $(OBJDIR)

$(SKOBJS): $(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj $(@B).cpp

$(FREEZE): ..\common\win32\$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj ..\common\win32\$(@B).cpp

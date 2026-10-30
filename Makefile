# Build with: nmake        Clean with: nmake clean
# Run from a Visual Studio developer command prompt (vcvars32.bat / vcvars64.bat).

CXXFLAGS = /nologo /EHsc
CFLAGS   = /nologo

PROGS = drra_svc.exe \
        send_test_report.exe \
        svcconfig.exe \
        svccontrol.exe \
        list_all_apps.exe \
        list_all_patches.exe \
        list_defender_alerts.exe \
        get_ip_address.exe \
        get_service_tag.exe

all: $(PROGS) drra_messages.dll

common.obj: common.cpp common.h agent_auth.h
	cl $(CXXFLAGS) /c common.cpp

agent_auth.obj: agent_auth.cpp agent_auth.h common.h
	cl $(CXXFLAGS) /c agent_auth.cpp

uploads.obj: uploads.cpp uploads.h agent_auth.h common.h
	cl $(CXXFLAGS) /c uploads.cpp

drra_svc.exe: drra_svc.cpp drra_messages.h common.h uploads.h common.obj agent_auth.obj uploads.obj
	cl $(CXXFLAGS) drra_svc.cpp common.obj agent_auth.obj uploads.obj

send_test_report.exe: send_test_report.cpp common.h common.obj agent_auth.obj
	cl $(CXXFLAGS) send_test_report.cpp common.obj agent_auth.obj

svcconfig.exe: svcconfig.cpp
	cl $(CFLAGS) svcconfig.cpp

svccontrol.exe: svccontrol.cpp
	cl $(CFLAGS) svccontrol.cpp

list_all_apps.exe: list_all_apps.cpp
	cl $(CXXFLAGS) list_all_apps.cpp

list_all_patches.exe: list_all_patches.cpp
	cl $(CXXFLAGS) list_all_patches.cpp

list_defender_alerts.exe: list_defender_alerts.cpp
	cl $(CXXFLAGS) list_defender_alerts.cpp

get_ip_address.exe: get_ip_address.cpp
	cl $(CXXFLAGS) get_ip_address.cpp

get_service_tag.exe: get_service_tag.cpp
	cl $(CXXFLAGS) get_service_tag.cpp

# Message DLL
drra_messages.rc drra_messages.h: drra_messages.mc
	mc -U drra_messages.mc

drra_messages.res: drra_messages.rc
	rc -r drra_messages.rc

drra_messages.dll: drra_messages.res
	link /nologo -dll -noentry -out:drra_messages.dll drra_messages.res

# Windows setup.exe in output\ (needs Inno Setup 6)
ISCC = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

installer: all
	$(ISCC) /Qp drra_installer.iss

# The programs, message DLL, default settings and docs in a zip file, for
# installs without setup.exe (see build.txt). Keep VERSION in step with
# AGENT_VERSION in common.h and AppVersion in drra_installer.iss.
VERSION = 3.4
BINZIP  = output\drra_$(VERSION)_bin_x64.zip

binzip: all
	-mkdir output 2>nul
	-del /q $(BINZIP) 2>nul
	tar -a -c -f $(BINZIP) $(PROGS) drra_messages.dll drra.ini README.md LICENSE

clean:
	-del /q *.exe *.obj *.bin 2>nul
	-del /q drra_messages.dll drra_messages.res drra_messages.rc 2>nul
	-rmdir /s /q output 2>nul

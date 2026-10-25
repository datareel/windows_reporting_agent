# Build with: nmake        Clean with: nmake clean
# Run from a Visual Studio developer command prompt (vcvars32.bat / vcvars64.bat).

CXXFLAGS = /nologo /EHsc
CFLAGS   = /nologo

PROGS = windows_reporting_agent_svc.exe \
        send_test_report.exe \
        svcconfig.exe \
        svccontrol.exe \
        list_all_apps.exe \
        list_all_patches.exe \
        list_defender_alerts.exe \
        get_ip_address.exe \
        get_service_tag.exe

all: $(PROGS) windows_reporting_agent.dll

common.obj: common.cpp common.h agent_auth.h
	cl $(CXXFLAGS) /c common.cpp

agent_auth.obj: agent_auth.cpp agent_auth.h common.h
	cl $(CXXFLAGS) /c agent_auth.cpp

windows_reporting_agent_svc.exe: windows_reporting_agent_svc.cpp windows_reporting_agent.h common.h common.obj agent_auth.obj
	cl $(CXXFLAGS) windows_reporting_agent_svc.cpp common.obj agent_auth.obj

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
windows_reporting_agent.rc windows_reporting_agent.h: windows_reporting_agent.mc
	mc -U windows_reporting_agent.mc

windows_reporting_agent.res: windows_reporting_agent.rc
	rc -r windows_reporting_agent.rc

windows_reporting_agent.dll: windows_reporting_agent.res
	link /nologo -dll -noentry -out:windows_reporting_agent.dll windows_reporting_agent.res

clean:
	-del /q *.exe *.obj *.bin 2>nul
	-del /q windows_reporting_agent.dll windows_reporting_agent.res windows_reporting_agent.rc 2>nul

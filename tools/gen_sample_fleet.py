#!/usr/bin/env python3
# Generate a sample fleet of DataReel Reporting Agent reports with made up
# hostnames, users, service tags and addresses, for the live demo, for
# testing the status page and the DRSM plugins, and for screenshots.
#
# The reports use the exact layout the agent writes. The fleet covers the
# states an admin needs to see: current, late and missing check-ins, empty
# and bad report files, computers behind on Windows updates, missing
# required programs, unwanted programs, and Windows Defender detections.
#
# Usage: gen_sample_fleet.py [--out DIR] [--count N] [--seed N] [--now EPOCH]
#                            [--agents-dir DIR]
#
#   --out         where to write <host>_windows_report_agent.txt (default
#                 ./sample_fleet)
#   --count       number of computers (default 48)
#   --seed        random seed, the same seed gives the same fleet (default 2026)
#   --now         report times are relative to this time (default: now)
#   --agents-dir  also write server registration records (<host>.json) for
#                 the admin page demo
#
# A fleet.json summary of each computer's scenario is written to --out.

import argparse
import datetime
import json
import os
import random
import string
import time

SCENARIOS = [
    # name, share of the fleet
    ("healthy", 0.46),
    ("late", 0.10),            # last report 2 to 14 days ago
    ("missing", 0.08),         # last report more than 15 days ago
    ("behind_patches", 0.10),  # last cumulative update months ago
    ("missing_apps", 0.08),    # required programs missing
    ("unwanted_app", 0.06),    # a program that should not be there
    ("defender", 0.08),        # malware detected / protection disabled
    ("empty", 0.02),           # placeholder file, agent never installed
    ("bad", 0.02),             # report without a time stamp
]

SURNAMES = ["smith", "garcia", "chen", "patel", "nguyen", "okafor", "muller", "rossi", "kim", "silva",
            "jones", "brown", "lopez", "khan", "wilson", "taylor", "moore", "clark", "lewis", "walker",
            "young", "hall", "allen", "king", "wright", "scott", "green", "baker", "adams", "nelson"]
INITIALS = "abcdefghjklmnprstw"

BASE_APPS = [
    ("Microsoft 365 Apps for enterprise - en-us", "16.0.19328.20190"),
    ("Google Chrome", "141.0.7390.123"),
    ("Microsoft Edge", "141.0.3537.92"),
    ("Microsoft Edge WebView2 Runtime", "141.0.3537.92"),
    ("Zoom Workplace (64-bit)", "6.6.4.62915"),
    ("7-Zip 25.01 (x64)", "25.01"),
    ("Adobe Acrobat Reader (64-bit)", "25.001.20813"),
    ("Microsoft Teams Meeting Add-in for Microsoft Office", "1.25.26101"),
    ("Microsoft Update Health Tools", "5.72.0.0"),
    ("Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.50.35719", "14.50.35719.0"),
    ("DataReel Reporting Agent for Windows", "3.3"),
]
BASE_APPS_32 = [
    ("Microsoft Visual C++ 2015-2022 Redistributable (x86) - 14.50.35719", "14.50.35719.0"),
    ("Microsoft OneDrive", "25.181.0914.0002"),
]
OPTIONAL_APPS = [
    ("Mozilla Firefox (x64 en-US)", "144.0.2"),
    ("Notepad++ (64-bit x64)", "8.8.6"),
    ("VLC media player", "3.0.21"),
    ("Python 3.13.7 (64-bit)", "3.13.7150.0"),
    ("Git", "2.51.1"),
    ("Microsoft Visual Studio Code", "1.105.1"),
    ("Cisco Secure Client - AnyConnect VPN", "5.1.12.146"),
    ("Dell Command | Update", "5.5.0"),
    ("Slack (Machine - MSI)", "4.46.101"),
    ("WinSCP 6.5.4", "6.5.4"),
    ("PuTTY release 0.83 (64-bit)", "0.83.0.0"),
    ("Microsoft Power BI Desktop (x64)", "2.148.1226.0"),
]
REQUIRED_APPS = ["Microsoft 365 Apps for enterprise - en-us", "Zoom Workplace (64-bit)", "Google Chrome"]
UNWANTED_APPS = [
    ("uTorrent", "3.6.0.47196"),
    ("TeamViewer 13", "13.2.26558"),
    ("CCleaner", "6.38"),
    ("WinRAR 5.40 (64-bit)", "5.40.0"),
]

# Monthly cumulative updates: (year, month, KB)
CUMULATIVE = [
    (2026, 3, 5094812), (2026, 4, 5098166), (2026, 5, 5103409), (2026, 6, 5108201),
    (2026, 7, 5112774), (2026, 8, 5117530), (2026, 9, 5122436), (2026, 10, 5127648),
]
DOTNET = [(2026, 6, 5108427), (2026, 9, 5122611)]

THREATS = [
    ("Trojan:Win32/Wacatac.B!ml", "Severe", "Trojan", "C:\\Users\\%s\\Downloads\\invoice_2026_10.zip->invoice.exe"),
    ("PUA:Win32/Presenoker", "Low", "PUA", "C:\\Users\\%s\\Downloads\\free_pdf_converter_setup.exe"),
    ("Trojan:JS/Redirector.QE!MTB", "Severe", "Trojan", "C:\\Users\\%s\\AppData\\Local\\Temp\\chrome_cache\\f_0003a1"),
    ("HackTool:Win32/Keygen", "High", "Tool", "C:\\Users\\%s\\Desktop\\tools\\keygen.exe"),
]


def second_tuesday(year, month):
    d = datetime.date(year, month, 1)
    first_tue = d + datetime.timedelta(days=(1 - d.weekday()) % 7)
    return first_tue + datetime.timedelta(days=7)


def make_hostnames(rng, count):
    names = []
    n_ws = max(2, count // 6)
    n_srv = 2 if count >= 20 else 0
    n_other = 2 if count >= 30 else 0
    n_lt = count - n_ws - n_srv - n_other
    for i in rng.sample(range(100, 400), n_lt):
        names.append("lt-%04d" % i)
    for i in rng.sample(range(10, 90), n_ws):
        names.append("ws-%04d" % i)
    names += ["srv-print01", "srv-files02"][:n_srv]
    names += ["kiosk-lobby", "conf-room-b"][:n_other]
    return sorted(names)


def pick_scenarios(rng, hosts):
    # Servers stay healthy so the demo is easy to read. Every other
    # scenario gets its share of the rest, at least one computer each.
    clients = [h for h in hosts if not h.startswith("srv-")]
    order = []
    for name, share in SCENARIOS:
        if name != "healthy":
            order += [name] * max(1, round(share * len(hosts)))
    order = order[:len(clients)]
    order += ["healthy"] * (len(clients) - len(order))
    rng.shuffle(order)
    result = dict(zip(clients, order))
    for h in hosts:
        result.setdefault(h, "healthy")
    return result


def ctime(epoch):
    # Same layout as the C ctime() the agent uses: "Sun Oct  5 19:58:08 2026"
    t = time.localtime(epoch)
    return "%s %2d %s" % (time.strftime("%a %b", t), t.tm_mday, time.strftime("%H:%M:%S %Y", t))


def iso(epoch):
    return datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f0Z")


def defender_event(rng, host, epoch, event_id, level, data):
    guid = lambda: "{%s}" % "-".join("".join(rng.choice("0123456789abcdef") for _ in range(n)) for n in (8, 4, 4, 4, 12))
    fields = "".join("<Data Name='%s'>%s</Data>" % (k, v) for k, v in data)
    return ("<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System>"
            "<Provider Name='Microsoft-Windows-Windows Defender' Guid='{11cd958a-c507-4ef3-b3f2-5fd9dfbd2c78}'/>"
            "<EventID>%d</EventID><Version>0</Version><Level>%d</Level><Task>0</Task><Opcode>0</Opcode>"
            "<Keywords>0x8000000000000000</Keywords><TimeCreated SystemTime='%s'/><EventRecordID>%d</EventRecordID>"
            "<Correlation ActivityID='%s'/><Execution ProcessID='%d' ThreadID='%d'/>"
            "<Channel>Microsoft-Windows-Windows Defender/Operational</Channel><Computer>%s</Computer>"
            "<Security UserID='S-1-5-18'/></System><EventData>%s</EventData></Event>"
            % (event_id, level, iso(epoch), rng.randint(1000, 9000), guid(), rng.randint(1000, 9000),
               rng.randint(100, 20000), host.upper(), fields))


def build_report(rng, host, scen, report_time, users):
    lines = []
    lines.append("DataReel Reporting Agent report from host %s" % host)
    lines.append("VERSION: 3.3")
    lines.append("HOSTNAME: %s" % host)
    if scen != "bad":
        lines.append("ETIME: %d" % report_time)
    lines.append("DATETIME: %s" % ctime(report_time))
    lines.append("")
    lines.append("")
    tag = "".join(rng.choice(string.ascii_uppercase + string.digits) for _ in range(7))
    lines.append("SERVICE TAG AND BARCODE:")
    lines.append("Dell Service Tag (Serial Number): %s" % tag)
    lines.append("Dell Asset Tag (Barcode): EX-%06d" % rng.randint(100000, 999999))
    lines.append("END SERVICE TAG AND BARCODE")
    lines.append("")
    lines.append("")
    lines.append("IP ADDRESS:")
    adapter = "Ethernet" if host.startswith(("ws-", "srv-", "kiosk", "conf")) else rng.choice(["Wi-Fi", "Wi-Fi", "Ethernet 2"])
    subnet = 30 if host.startswith("srv-") else rng.choice([20, 21, 22, 40])
    lines.append("Adapter: %s" % adapter)
    lines.append("IPv4 Address: 10.%d.%d.%d" % (subnet, rng.randint(0, 3), rng.randint(10, 250)))
    lines.append("")
    if host.startswith("lt-") and rng.random() < 0.3:
        lines.append("Adapter: Cisco Secure Client Virtual Miniport Adapter for Windows x64")
        lines.append("IPv4 Address: 172.16.%d.%d" % (rng.randint(0, 15), rng.randint(2, 250)))
        lines.append("")
    lines.append("END IP ADDRESS")
    lines.append("")
    lines.append("USERS:")
    profiles = ["All Users", "Default", "Default User", "Public"] + users
    if host.startswith("srv-"):
        profiles += ["Administrator", "svc_backup"]
    for p in sorted(profiles, key=str.lower):
        lines.append("[DIR]  %s" % p)
    lines.append("[FILE] desktop.ini")
    lines.append("END USERS")
    lines.append("")

    # Programs
    apps = list(BASE_APPS)
    apps += rng.sample(OPTIONAL_APPS, rng.randint(1, 5))
    missing = []
    if scen == "missing_apps":
        missing = rng.sample(REQUIRED_APPS, rng.randint(1, 2))
        apps = [a for a in apps if a[0] not in missing]
    unwanted = None
    if scen == "unwanted_app":
        unwanted = rng.choice(UNWANTED_APPS)
        apps.append(unwanted)
    if host.startswith("srv-"):
        apps = [a for a in apps if not a[0].startswith(("Zoom", "Microsoft 365", "Adobe", "Microsoft Teams"))]
    rng.shuffle(apps)
    lines.append("ALL INSTALLED PROGRAMS:")
    lines.append("--- 64-bit / Default Hive ---")
    for name, ver in apps:
        lines.append("Name: %s | Version: %s" % (name, ver))
    lines.append("")
    lines.append("--- 32-bit Wow6432Node Hive ---")
    for name, ver in BASE_APPS_32:
        lines.append("Name: %s | Version: %s" % (name, ver))
    lines.append("END ALL INSTALLED PROGRAMS")
    lines.append("")

    # Defender alerts in the 30 days before the report
    lines.append("WINDOWS DEFENDER ALERTS:")
    threat = None
    if scen == "defender":
        kind = rng.choice(["detected", "detected", "disabled"])
        user = users[0] if users else "Public"
        if kind == "detected":
            threat = rng.choice(THREATS)
            t = report_time - rng.randint(1, 20) * 86400 - rng.randint(0, 80000)
            common = [("Product Name", "Microsoft Defender Antivirus"), ("Product Version", "4.18.26090.9"),
                      ("Threat Name", threat[0]), ("Severity Name", threat[1]), ("Category Name", threat[2]),
                      ("Path", "file:_" + threat[3] % user), ("Detection User", "EXAMPLE\\%s" % user)]
            lines.append(defender_event(rng, host, t, 1116, 3, common + [("Action Name", "Not Applicable")]))
            lines.append("")
            if rng.random() < 0.6:
                lines.append(defender_event(rng, host, t + 40, 1117, 3, common + [("Action Name", "Quarantine")]))
            else:
                lines.append(defender_event(rng, host, t + 40, 1118, 2, common + [("Action Name", "Quarantine"),
                                                                                 ("Error Description", "The process cannot access the file because it is being used by another process.")]))
            lines.append("")
            threat = threat[0]
        else:
            t = report_time - rng.randint(1, 10) * 86400
            lines.append(defender_event(rng, host, t, 5001, 3, [("Product Name", "Microsoft Defender Antivirus"),
                                                                 ("Product Version", "4.18.26090.9")]))
            lines.append("")
            threat = "Real-time protection disabled"
    if rng.random() < 0.25:
        t = report_time - rng.randint(1, 28) * 86400
        lines.append(defender_event(rng, host, t, 1002, 3, [("Product Name", "Microsoft Defender Antivirus"),
                                                             ("Scan Type", "Antimalware"), ("Scan Parameters", "Quick Scan"),
                                                             ("Stop Reason", "RPC connection rundown")]))
        lines.append("")
    lines.append("END WINDOWS DEFENDER ALERTS")
    lines.append("")
    lines.append("")

    # Patches: cumulative updates up to a cutoff, newest first
    report_date = datetime.date.fromtimestamp(report_time)
    cutoff = report_date
    if scen == "behind_patches":
        cutoff = report_date - datetime.timedelta(days=rng.choice([65, 95, 130]))
    patches = []
    for y, m, kb in CUMULATIVE:
        d = second_tuesday(y, m) + datetime.timedelta(days=rng.randint(0, 6))
        if d <= cutoff:
            ver = "26H2" if (y, m) >= (2026, 9) else "25H2"
            patches.append((d, "%d-%02d Cumulative Update for Windows 11 Version %s for x64-based Systems (KB%d)" % (y, m, ver, kb), kb))
    for y, m, kb in DOTNET:
        d = second_tuesday(y, m) + datetime.timedelta(days=rng.randint(0, 6))
        if d <= cutoff:
            patches.append((d, "%d-%02d Cumulative Update for .NET Framework 3.5 and 4.8.1 for Windows 11 (KB%d)" % (y, m, kb), kb))
    for i in range(rng.randint(2, 5)):
        d = min(report_date, cutoff + datetime.timedelta(days=30)) - datetime.timedelta(days=i * rng.randint(1, 3))
        patches.append((d, "Security Intelligence Update for Microsoft Defender Antivirus - KB2267602 (Version 1.461.%d.0) - Current Channel (Broad)" % (190 - i * 4), None))
    patches.sort(key=lambda p: p[0], reverse=True)
    lines.append("INSTALLED WINDOWS PATCHES:")
    for d, title, _ in patches:
        lines.append("Date:  %d-%d-%d" % (d.year, d.month, d.day))
        lines.append(" Patch: %s" % title)
        lines.append("")
    lines.append("END INSTALLED WINDOWS PATCHES")
    lines.append("")

    lines.append("HOT FIXES:")
    lines.append("Installed Windows Hotfixes:")
    lines.append("--------------------------")
    for d, title, kb in sorted([p for p in patches if p[2]], key=lambda p: p[2]):
        lines.append("Hotfix ID:      KB%d" % kb)
        lines.append("Description:    %s" % ("Security Update" if "Cumulative Update for Windows" in title else "Update"))
        lines.append("Installed By:   NT AUTHORITY\\SYSTEM")
        lines.append("Installed On:   %d/%d/%d" % (d.month, d.day, d.year))
        lines.append("--------------------------")
    lines.append("END HOT FIXES")
    lines.append("")
    lines.append("")

    last_cu = max([p[0] for p in patches if p[2] and "Windows 11" in p[1]], default=None)
    info = {"missing_apps": missing, "unwanted_app": unwanted[0] if unwanted else None,
            "defender": threat, "last_cumulative_update": last_cu.isoformat() if last_cu else None,
            "service_tag": tag, "users": users}
    return "\n".join(lines), info


def main():
    ap = argparse.ArgumentParser(description="Generate a sample fleet of DataReel Reporting Agent reports")
    ap.add_argument("--out", default="sample_fleet")
    ap.add_argument("--count", type=int, default=48)
    ap.add_argument("--seed", type=int, default=2026)
    ap.add_argument("--now", type=int, default=int(time.time()))
    ap.add_argument("--agents-dir", default="")
    args = ap.parse_args()

    rng = random.Random(args.seed)
    # Separate stream for the registration records, so the reports are the
    # same with or without --agents-dir
    reg_rng = random.Random(args.seed + 1)
    os.makedirs(args.out, exist_ok=True)
    hosts = make_hostnames(rng, args.count)
    scenarios = pick_scenarios(rng, hosts)
    summary = {"generated": args.now, "seed": args.seed, "hosts": []}

    for host in hosts:
        scen = scenarios[host]
        if scen == "late":
            report_time = args.now - rng.randint(2, 14) * 86400 - rng.randint(0, 80000)
        elif scen == "missing":
            report_time = args.now - rng.randint(16, 60) * 86400 - rng.randint(0, 80000)
        else:
            report_time = args.now - rng.randint(60, 45 * 60)   # within the last 45 minutes
        users = [rng.choice(INITIALS) + rng.choice(SURNAMES) for _ in range(1 if host.startswith("lt-") else rng.randint(1, 3))]
        path = os.path.join(args.out, "%s_windows_report_agent.txt" % host)
        entry = {"hostname": host, "scenario": scen}
        if scen == "empty":
            open(path, "w").close()
            report_time = args.now - rng.randint(20, 40) * 86400
        else:
            text, info = build_report(rng, host, scen, report_time, users)
            with open(path, "w") as f:
                f.write(text)
            entry.update(info)
            entry["report_time"] = report_time
        os.utime(path, (report_time, report_time))
        summary["hosts"].append(entry)

        if args.agents_dir and scen != "empty":
            os.makedirs(args.agents_dir, exist_ok=True)
            reg = report_time - reg_rng.randint(30, 120) * 86400
            status = "pending" if reg_rng.random() < 0.05 else "approved"
            fp = "".join(reg_rng.choice("0123456789abcdef") for _ in range(64))
            when = lambda e: time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime(e))
            record = {"hostname": host, "status": status, "key_fingerprint": fp,
                      "cert_pem": "(sample: no certificate)", "registered_at": when(reg),
                      "approved_at": when(reg + 600) if status == "approved" else "",
                      "approved_by": reg_rng.choice(["enrollment token", "admin"]) if status == "approved" else "",
                      "last_register": when(reg), "last_seen": when(report_time) if status == "approved" else "",
                      "last_addr": "10.%d.%d.%d" % (reg_rng.randint(20, 40), reg_rng.randint(0, 3), reg_rng.randint(10, 250))}
            with open(os.path.join(args.agents_dir, host + ".json"), "w") as f:
                json.dump(record, f, indent=4)

    with open(os.path.join(args.out, "fleet.json"), "w") as f:
        json.dump(summary, f, indent=2)
    counts = {}
    for h in summary["hosts"]:
        counts[h["scenario"]] = counts.get(h["scenario"], 0) + 1
    print("Wrote %d reports to %s: %s" % (len(hosts), args.out, ", ".join("%s %d" % kv for kv in sorted(counts.items()))))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# Generate the HTML documentation in docs/ (same style as the DRSM docs).
# Edit the page content here, then run: python3 tools/gen_docs.py docs
import os, sys

OUT = sys.argv[1]
DL = "https://datareel.com/downloads/windows_reporting_agent/"
DEMO = "https://datareel.com/windows_reporting_agent/demo/"

ICON_MENU = '<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M4 6h16M4 12h16M4 18h16"/></svg>'
ICON_DEMO = '<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/></svg>'
ICON_DL = '<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3v12M7 10l5 5 5-5M5 21h14"/></svg>'
ICON_THEME = '<svg class="icon sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"/></svg><svg class="icon moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>'

NAV = [
    ("Get started", [("index.html", "Overview"), ("install.html", "Install the agent"), ("server.html", "Set up the report server")]),
    ("Using the agent", [("approve.html", "Approve computers"), ("configuration.html", "Configuration"), ("reports.html", "Reports and status page")]),
    ("Reference", [("security.html", "Security"), ("building.html", "Build from source"), ("troubleshooting.html", "Troubleshooting"), ("changelog.html", "What's new")]),
]
ORDER = [f for _, items in NAV for f, _ in items]
LABELS = {f: l for _, items in NAV for f, l in items}

def code(lines):
    """lines: list of (kind, text) kind in '', '#', '$', 'c'"""
    out = []
    for kind, text in lines:
        t = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        if kind == "#":
            out.append('<span class="r"># </span>' + t)
        elif kind == "$":
            out.append('<span class="p">$ </span>' + t)
        elif kind == "c":
            out.append('<span class="c">' + t + '</span>')
        else:
            out.append(t)
    return "<pre><code>" + "\n".join(out) + "</code></pre>"

def table(head, rows):
    h = "".join("<th>%s</th>" % c for c in head)
    b = "".join("<tr>" + "".join("<td>%s</td>" % c for c in r) + "</tr>" for r in rows)
    return '<div class="table-wrap"><table><thead><tr>%s</tr></thead><tbody>%s</tbody></table></div>' % (h, b)

def page(fname, title, desc, toc, body):
    side = []
    for group, items in NAV:
        side.append("<h4>%s</h4>" % group)
        for f, label in items:
            if f == fname:
                side.append('<a href="%s" class="active" aria-current="page">%s</a>' % (f, label))
                if toc:
                    side.append('<div class="toc">' + "".join('<a href="#%s">%s</a>' % t for t in toc) + "</div>")
            else:
                side.append('<a href="%s">%s</a>' % (f, label))
    side.append("<h4>Links</h4>")
    if DEMO:
        side.append('<a href="%s">Live demo</a>' % DEMO)
    side.append('<a href="%s">Downloads</a><a href="https://datareel.com/contact.php">Report an issue</a>' % DL)

    i = ORDER.index(fname)
    pager = []
    if i > 0:
        pager.append('<a class="card prev" href="%s"><small>Previous</small>%s</a>' % (ORDER[i - 1], LABELS[ORDER[i - 1]]))
    if i < len(ORDER) - 1:
        pager.append('<a class="card next" href="%s"><small>Next</small>%s</a>' % (ORDER[i + 1], LABELS[ORDER[i + 1]]))

    demo_btn = ('<a class="tool primary" href="%s">%s<span>Live demo</span></a>' % (DEMO, ICON_DEMO)) if DEMO else ""
    full_title = "DataReel Reporting Agent Documentation" if fname == "index.html" else "%s | DataReel Reporting Agent Documentation" % title
    html = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(full_title)s</title>
<meta name="description" content="%(desc)s">
<link rel="icon" href="images/logo.svg" type="image/svg+xml">
<link rel="stylesheet" href="docs.css">
<script src="docs.js"></script>
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<header class="topbar">
  <div class="topbar-inner">
    <button class="tool icon-only menu-btn" type="button" data-action="menu" aria-label="Menu" aria-expanded="false">%(menu)s</button>
    <a class="brand" href="index.html"><img src="images/logo.svg" alt="">DataReel Reporting Agent <small>Documentation</small></a>
    <span class="spacer"></span>
    %(demo)s
    <a class="tool hide-sm" href="%(dl)s">%(dlicon)s<span>Downloads</span></a>
    <button class="tool icon-only" type="button" data-action="theme" aria-label="Switch light or dark theme">%(theme)s</button>
  </div>
</header>
<div class="layout">
  <nav class="sidebar" aria-label="Documentation">
    %(side)s
  </nav>
  <main class="content" id="main">
%(body)s
<nav class="pager" aria-label="Previous and next page">%(pager)s</nav>
  </main>
</div>
<footer class="foot">DataReel Reporting Agent for Windows 3.3 &middot; GNU General Public License v3 &middot; <a href="https://datareel.com/info_windows_reporting_agent.php">datareel.com</a></footer>
</body>
</html>
""" % dict(full_title=full_title, desc=desc, menu=ICON_MENU, demo=demo_btn, dl=DL, dlicon=ICON_DL,
           theme=ICON_THEME, side="".join(side), body=body.strip(), pager="".join(pager))
    with open(os.path.join(OUT, fname), "w") as f:
        f.write(html)

DIAGRAM = """<svg class="diagram" viewBox="0 0 860 330" role="img" aria-labelledby="dg-t">
<title id="dg-t">How the DataReel Reporting Agent works: each Windows computer runs the agent service, which registers its certificate and uploads signed reports to the PHP pages on a report server, or copies them to a file share or SSH server. The reports feed a status page, the admin page and DRSM.</title>
<defs>
  <marker id="a1" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path class="ah" d="M0 0L10 5L0 10z"/></marker>
  <marker id="a2" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path class="ah hi" d="M0 0L10 5L0 10z"/></marker>
  <marker id="a3" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path class="ah alt" d="M0 0L10 5L0 10z"/></marker>
</defs>
<rect class="box hi" x="20" y="20" width="250" height="150" rx="14"/>
<text class="t" x="40" y="50">Windows computers</text>
<text class="s" x="40" y="70">laptops, desktops, servers</text>
<text class="m" x="40" y="98">DataReelReportAgent service</text>
<text class="s" x="40" y="120">inventory every 15 minutes</text>
<text class="s" x="40" y="138">machine key + certificate</text>

<rect class="box" x="370" y="20" width="250" height="150" rx="14"/>
<text class="t" x="390" y="50">Report server</text>
<text class="s" x="390" y="70">Apache + PHP, local or hosted</text>
<text class="m" x="390" y="98">register.php</text>
<text class="m" x="390" y="120">upload.php</text>
<text class="m" x="390" y="142">admin.php</text>

<rect class="box" x="370" y="230" width="250" height="80" rx="14"/>
<text class="t" x="390" y="260">Reports</text>
<text class="s" x="390" y="284">one text file per computer</text>

<rect class="box alt" x="690" y="20" width="150" height="150" rx="14"/>
<text class="t" x="708" y="50">Admins</text>
<text class="s" x="708" y="74">approve or</text>
<text class="s" x="708" y="92">revoke computers</text>

<rect class="box alt" x="690" y="230" width="150" height="80" rx="14"/>
<text class="t" x="708" y="260">Status page</text>
<text class="s" x="708" y="284">and DRSM</text>

<rect class="box" x="20" y="230" width="250" height="80" rx="14"/>
<text class="t" x="40" y="260">Or a file share / SSH</text>
<text class="s" x="40" y="284">CIFS copy, scp</text>

<path class="ln hi" d="M270 95H368" marker-end="url(#a2)"/>
<text class="lbl" x="319" y="86" text-anchor="middle">signed</text>
<text class="lbl" x="319" y="112" text-anchor="middle">HTTPS</text>
<path class="ln" d="M495 170V228" marker-end="url(#a1)"/>
<path class="ln alt" d="M688 95H622" marker-end="url(#a3)"/>
<path class="ln alt" d="M620 270H688" marker-end="url(#a3)"/>
<path class="ln" d="M145 170V228" marker-end="url(#a1)"/>
</svg>"""

# ---------------------------------------------------------------- index
page("index.html", "Overview",
     "DataReel Reporting Agent documentation: hardware, software, patch and Windows Defender inventory for Windows computers without an enterprise management suite.",
     [("what", "What it reports"), ("how", "How it works"), ("who", "Who it is for"), ("start", "Get started"), ("docs", "Other documentation")],
     """
<section class="hero">
  <span class="badge new">New October 2026</span>
  <h1 style="margin-top:14px">DataReel Reporting Agent</h1>
  <p class="lead">A small Windows service that sends a hardware, software, patch and Windows Defender inventory from every computer to your report server. Built for sites that need to know what is on their Windows computers but do not run SCCM/MCM, Intune or BigFix.</p>
  <div class="actions">
    <a class="tool primary" href="https://datareel.com/windows_reporting_agent/demo/">Open the live demo</a>
    <a class="tool" href="install.html">Install the agent</a>
    <a class="tool" href="server.html">Set up the report server</a>
    <a class="tool" href="changelog.html">What's new</a>
  </div>
</section>

<h2 id="what">What it reports</h2>
<div class="grid">
  <div class="card"><h3>Hardware</h3><p>Computer name, service tag (the Dell service tag on Dell computers), asset tag, and the IPv4 addresses of the adapters that are up.</p></div>
  <div class="card"><h3>Software</h3><p>Name and version of every installed program from the 64-bit and 32-bit uninstall registry keys.</p></div>
  <div class="card"><h3>Patches</h3><p>The Windows Update installation history and the installed hotfixes.</p></div>
  <div class="card"><h3>Windows Defender</h3><p>Critical, error and warning events from the Defender event log for the last 30 days.</p></div>
  <div class="card"><h3>Users</h3><p>The profile folders in <code>C:\\Users</code>, to see who has signed in to the computer.</p></div>
  <div class="card"><h3>Check-ins</h3><p>A status page shows which computers reported in the last day, in the last 15 days, or not at all.</p></div>
</div>

<h2 id="how">How it works</h2>
<p>The agent runs as the <code>DataReelReportAgent</code> service. Every 15 minutes (<code>report_interval_minutes</code>) it writes a plain text report and delivers it. On its first run it creates a key that cannot be exported and registers its certificate with the report server. After an admin approves the computer, or it sends the right enrollment token, every report it uploads is signed, and the server only accepts reports that are signed by the computer they claim to be from.</p>
""" + DIAGRAM + """
<p>The report can also, or instead, be copied to a Windows file share or to an SSH server. The reports are plain text files, one per computer, which the fleet status page and the <a href="https://datareel.com/info_drsm.php">DataReel System Monitor</a> read.</p>

<h2 id="who">Who it is for</h2>
<p>Enterprise suites do this and much more, but they need servers, licenses and staff. The agent covers the everyday questions with a 2.5 MB installer and a few PHP pages:</p>
<ul>
  <li>Is every laptop that was handed out actually checking in?</li>
  <li>Which computers are missing a program, or still have one that should be gone?</li>
  <li>Which computers are behind on Windows updates?</li>
  <li>Has Windows Defender found anything this month?</li>
  <li>Which service tag belongs to which computer name?</li>
</ul>
<p>It was first written to sort out a deployment of 120 laptops with inventory gaps, missing software and patching problems, before an enterprise suite was working.</p>

<h2 id="start">Get started</h2>
<div class="grid">
  <a class="card" href="server.html"><h3>1. Set up the report server</h3><p>Copy the PHP pages to an Apache and PHP host with <code>install_server.sh</code>. Shared hosting works.</p></a>
  <a class="card" href="install.html"><h3>2. Install the agent</h3><p>Run the setup on each computer, or deploy it silently with Group Policy, PDQ or a script.</p></a>
  <a class="card" href="approve.html"><h3>3. Approve the computers</h3><p>On the admin page, or automatically with an enrollment token.</p></a>
</div>

<h2 id="docs">Other documentation</h2>
<p>These pages cover the same ground as the files that ship with the agent:</p>
<ul>
  <li><code>README.md</code>, the full install and configuration guide</li>
  <li><code>drra.ini</code>, every agent setting with comments</li>
  <li><code>php/server.ini.example</code>, every server setting with comments</li>
</ul>
""")

# ---------------------------------------------------------------- install
page("install.html", "Install the agent",
     "Install the DataReel Reporting Agent with the setup wizard, silently with Group Policy or PDQ, upgrade it and uninstall it.",
     [("requirements", "Requirements"), ("setup", "Run the setup"), ("silent", "Unattended installs"), ("check", "Check it works"), ("upgrade", "Upgrade"), ("uninstall", "Uninstall")],
     """
<h1>Install the agent</h1>
<p class="lead">Run the setup on each computer, or push it out silently with the settings on the command line. Set up the <a href="server.html">report server</a> first so you know its address.</p>

<h2 id="requirements">Requirements</h2>
""" + table(["Item", "Needs"], [
    ["Windows", "Windows 10 or 11, or Windows Server 2016 or later, 64-bit"],
    ["Rights", "An administrator account to install. The service runs as LocalSystem."],
    ["Network", "HTTPS access to the report server, or a file share or SSH server for the other delivery methods"],
    ["SCP delivery", "The Windows OpenSSH client, installed by default on Windows 10 1809 and later"],
]) + """

<h2 id="setup">Run the setup</h2>
<p>Download <code>drra_setup_3.3.exe</code> from the <a href=\"""" + DL + """">downloads page</a> and run it as an administrator. After the license and install folder pages it asks for the report server:</p>
""" + table(["Field", "Example", "Notes"], [
    ["Server host name", "<code>reports.example.com</code>", "The web server that runs the PHP pages"],
    ["Port", "<code>443</code>", "443 for HTTPS"],
    ["Web path of the upload pages folder", "<code>/windows_reporting_agent/upload</code>", "The folder that holds <code>upload.php</code> and <code>register.php</code>"],
    ["Enrollment token", "(from <code>server.ini</code>)", "Leave blank to approve the computer on the admin page"],
    ["Connection security", "HTTPS", "Plain HTTP is only for a test server"],
]) + """
<p>The setup installs to <code>C:\\Program Files\\DataReel\\ReportingAgent</code>, writes the settings to <code>drra.ini</code> there, creates the <code>DataReelReportAgent</code> service (automatic start, restarted if it stops) and starts it. The first report goes out right away. It also limits <code>C:\\ProgramData\\DataReel\\ReportingAgent</code>, where the agent keeps its certificate, report and log, to SYSTEM and Administrators.</p>

<h2 id="silent">Unattended installs</h2>
<p>For Group Policy, PDQ Deploy, a login script or any other deployment tool, pass the settings on the command line:</p>
""" + code([("", "drra_setup_3.3.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART ^"),
            ("", "    /SERVER=reports.example.com /PORT=443 /URLPATH=/windows_reporting_agent/upload ^"),
            ("", "    /TOKEN=<enrollment token>")]) + """
""" + table(["Switch", "Meaning"], [
    ["<code>/SERVER=</code>", "Report server host name"],
    ["<code>/PORT=</code>", "Port, 443 for HTTPS"],
    ["<code>/URLPATH=</code>", "Web path of the folder with <code>upload.php</code> and <code>register.php</code>"],
    ["<code>/TOKEN=</code>", "Enrollment token for automatic approval"],
    ["<code>/HTTP</code>", "Use plain HTTP (test servers only)"],
    ["<code>/METHODS=</code>", "Delivery methods, for example <code>cifs</code> or <code>https,cifs</code>"],
    ["<code>/CIFSSHARE=</code>", "File share, for example <code>\\\\fileserver\\reports</code>"],
    ["<code>/CIFSUSER=</code>, <code>/CIFSPASS=</code>", "Share login for shares outside a Windows domain. The password is stored encrypted. Setup logs record the command line, so do not combine <code>/CIFSPASS=</code> with <code>/LOG=</code>"],
    ["<code>/INTERVAL=</code>", "Minutes between reports, default 15"],
    ["<code>/LOG=C:\\path\\setup.log</code>", "Keep a setup log"],
]) + """
<div class="callout"><p>Settings you leave out keep the values of an existing install, so a new version can be pushed out with only <code>/VERYSILENT /SUPPRESSMSGBOXES /NORESTART</code>.</p></div>

<h2 id="check">Check it works</h2>
<p>Open <code>C:\\ProgramData\\DataReel\\ReportingAgent\\drra.log</code> as an administrator. After the first report it shows one line per delivery method:</p>
""" + code([("c", "Waiting for approval:"),
            ("", "[https] Registration status from server: pending"),
            ("", "ERROR - Report not sent, this agent is pending on the server. Approve host"),
            ("", "pc-0142 with key fingerprint 8d3f5723...3aa16 on the admin page"),
            ("", ""),
            ("c", "Approved and sent:"),
            ("", "[https] Upload successful"),
            ("", "STATUS: ok")]) + """
<p>To send a report now instead of waiting for the next 15-minute cycle, run <code>send_test_report.exe</code> from an administrator command prompt in the install folder. It skips the patch and hotfix sections, which are slow to collect.</p>

<h2 id="upgrade">Upgrade</h2>
<p>Run the new setup over the old one, interactively or with <code>/VERYSILENT</code>. It stops the service, replaces the programs, keeps <code>drra.ini</code> and starts the service again. The computer keeps its key, so it does not need to be approved again.</p>
<p>Earlier test builds were called Windows Reporting Agent, with the service <code>WindowsReportAgent</code> in <code>C:\\Program Files\\WindowsReportAgent</code>. Setup moves such an install over by itself: the settings (with their paths changed and the old 45-minute default changed to 15), the certificate, registration state, share password and machine key move to the DataReel names, and the old service, uninstall entry, event log source and folders are removed. If an Explorer window has the old folder open, the empty folder is deleted at the next reboot.</p>

<h2 id="uninstall">Uninstall</h2>
<p>Use <strong>Settings &gt; Apps</strong>, or run <code>unins000.exe /VERYSILENT</code> in the install folder. The service and programs are removed. The INI file and <code>C:\\ProgramData\\DataReel\\ReportingAgent</code> are kept, so a reinstall reuses the same key and settings. Delete them by hand to remove the agent completely, and delete the computer's registration on the <a href="approve.html">admin page</a>.</p>
""")

# ---------------------------------------------------------------- server
page("server.html", "Set up the report server",
     "Install the DataReel Reporting Agent PHP pages on Apache, set the permissions, protect the admin page and set the enrollment token.",
     [("requirements", "Requirements"), ("install", "Install the pages"), ("permissions", "Permissions"), ("apache", "Apache"), ("drsm", "DRSM dashboard login"), ("token", "Enrollment token"), ("shared", "Shared hosting"), ("https", "HTTPS")],
     """
<h1>Set up the report server</h1>
<p class="lead">The server side is four PHP files and a data directory. <code>install_server.sh</code> puts them in place. Plan on about 15 minutes.</p>
<div class="callout"><p>In the examples, <code>$</code> commands run as your own account and <code>#</code> commands run as root.</p></div>

<h2 id="requirements">Requirements</h2>
""" + table(["Item", "Needs"], [
    ["Web server", "Apache with <code>AllowOverride All</code> (or <code>FileInfo AuthConfig Limit</code>) for the pages folder"],
    ["PHP", "PHP 7.4 or later, PHP 8 supported, with the OpenSSL extension"],
    ["TLS", "A certificate the Windows computers trust, for example from Let's Encrypt"],
]) + """

<h2 id="install">Install the pages</h2>
<p>The pages are in the <code>php</code> folder of the source package. The script only shows what it would do until you add <code>--apply</code>:</p>
""" + code([("$", "cd windows_reporting_agent/php"),
            ("$", "./install_server.sh /var/www/html/windows_reporting_agent /var/lib/windows_reporting_agent"),
            ("#", "./install_server.sh --apply --admin-user admin \\"),
            ("", "      /var/www/html/windows_reporting_agent /var/lib/windows_reporting_agent")]) + """
""" + table(["Argument", "Gets"], [
    ["<code>WEB_DIR</code>", "An <code>upload</code> folder with <code>register.php</code>, <code>upload.php</code>, <code>admin.php</code>, <code>wra_lib.php</code> and an <code>.htaccess</code> file"],
    ["<code>DATA_DIR</code>", "<code>server.ini</code>, the agent registrations, replay protection data, <code>admin.htpasswd</code>, and the reports in <code>reports/</code>"],
]) + """
<p>The script never overwrites an existing <code>server.ini</code> or <code>admin.htpasswd</code>, so it is safe to run again to update the pages. <code>--admin-user</code> creates the admin login and asks for its password.</p>

<h2 id="permissions">Permissions</h2>
<p>The web server user must be able to write to <code>DATA_DIR</code>:</p>
""" + code([("#", "setfacl -R -m u:apache:rwX -m d:u:apache:rwX /var/lib/windows_reporting_agent")]) + """
<p>With SELinux enforcing (RHEL, Rocky, Alma, Fedora), also label it for writing:</p>
""" + code([("#", 'semanage fcontext -a -t httpd_sys_rw_content_t "/var/lib/windows_reporting_agent(/.*)?"'),
            ("#", "restorecon -R /var/lib/windows_reporting_agent")]) + """
<p>On Debian and Ubuntu the web server user is <code>www-data</code>.</p>

<h2 id="apache">Apache</h2>
<p>The generated <code>upload/.htaccess</code> does three things:</p>
""" + code([("", "SetEnv WRA_SERVER_INI /var/lib/windows_reporting_agent/server.ini"),
            ("", ""),
            ("", '<FilesMatch "^wra_(lib|page)\\.php$">'),
            ("", "    Require all denied"),
            ("", "</FilesMatch>"),
            ("", ""),
            ("", '<FilesMatch "^(admin|status|report)\\.php$">'),
            ("", "    AuthType Basic"),
            ("", '    AuthName "DataReel Reporting Agent admin"'),
            ("", "    AuthUserFile /var/lib/windows_reporting_agent/admin.htpasswd"),
            ("", "    Require valid-user"),
            ("", "</FilesMatch>")]) + """
<p>If you do not allow <code>.htaccess</code> files, put the same lines in a <code>&lt;Directory&gt;</code> block in the Apache configuration. <code>admin.php</code>, <code>status.php</code> and <code>report.php</code> refuse to run if the web server has not logged the user in, unless you set <code>require_remote_user = 0</code> in <code>server.ini</code>. Add or change admin logins with:</p>
""" + code([("$", "htpasswd -B /var/lib/windows_reporting_agent/admin.htpasswd admin")]) + """

<h2 id="drsm">DRSM dashboard login</h2>
<p>If the pages sit under a <a href="https://datareel.com/info_drsm.php">DataReel System Monitor</a> dashboard, for example in <code>/dashboard/windows</code>, they can use the dashboard login instead of a separate one. Install them with <code>--drsm-login</code> and the dashboard's config file:</p>
""" + code([("$", "./install_server.sh --apply --drsm-login ../../config/drsm.php \\"),
            ("", "      /var/www/html/dashboard/windows /var/www/html/dashboard/windows/data")]) + """
<p>This sets <code>login = drsm</code> and <code>drsm_config</code> in <code>server.ini</code> and leaves the Basic login out of <code>.htaccess</code>. People signed in to the dashboard open <code>admin.php</code>, <code>status.php</code> and <code>report.php</code> directly; anyone else goes to the dashboard login and back. Admin actions record the dashboard user name. Internal networks and a dashboard with its login turned off behave as they do on the dashboard. <code>register.php</code> and <code>upload.php</code> stay open to the agents.</p>

<h2 id="token">Enrollment token</h2>
<p>Without a token, every new computer waits on the <a href="approve.html">admin page</a> until you approve it. To approve computers automatically, set a long random token in <code>DATA_DIR/server.ini</code> and give the same token to the setup:</p>
""" + code([("$", "openssl rand -hex 24"),
            ("", ""),
            ("c", "; DATA_DIR/server.ini"),
            ("", "[registration]"),
            ("", "enrollment_token = 6f1c...e9a2"),
            ("", "auto_approve_with_token = 1")]) + """
<p>With <code>auto_approve_with_token = 0</code> the token is still checked, but an admin approves each computer. A computer that sends a wrong token is refused. Change the token after a deployment if it may have been seen by others: computers already approved keep working.</p>

<h2 id="shared">Shared hosting</h2>
<p>On a host such as HostGator where everything is under <code>public_html</code>, put <code>DATA_DIR</code> inside <code>WEB_DIR</code>. The script then adds an <code>.htaccess</code> with <code>Require all denied</code> to it so the reports, registrations and settings cannot be downloaded:</p>
""" + code([("$", "./install_server.sh --apply --admin-user admin \\"),
            ("", "      ~/public_html/windows_reporting_agent ~/public_html/windows_reporting_agent/data")]) + """
<div class="callout warn"><p>Open <code>https://your.site/windows_reporting_agent/data/server.ini</code> in a browser after the install. It must be refused (403). If it downloads, your host does not honor <code>.htaccess</code> there: move <code>DATA_DIR</code> outside <code>public_html</code>.</p></div>

<h2 id="https">HTTPS</h2>
<p>The agent uses the Windows certificate store to check the server, so the server needs a certificate from a public authority or from your own authority that is deployed to the computers. A self-signed server certificate fails with WinINet error 12175 or 12045. Plain HTTP works for a test server, but the inventory then crosses the network in clear text.</p>
""")

# ---------------------------------------------------------------- approve
page("approve.html", "Approve computers",
     "Approve, revoke and delete DataReel Reporting Agent computers on the admin page.",
     [("page", "The admin page"), ("states", "States"), ("actions", "Actions"), ("reinstall", "Reinstalled computers")],
     """
<h1>Approve computers</h1>
<p class="lead">A computer only sends reports after the server approves its certificate. Approve it on the admin page, or automatically with an <a href="server.html#token">enrollment token</a>.</p>

<h2 id="page">The admin page</h2>
<p>Open <code>admin.php</code> in the upload pages folder, for example <code>https://reports.example.com/windows_reporting_agent/upload/admin.php</code>, and log in, with the web server login or, with <code>login = drsm</code>, the <a href="server.html#drsm">DRSM dashboard login</a>. It lists every registered computer with its status, key fingerprint, registration and approval times, last report and last address, and the reports on disk that have no registration.</p>

<h2 id="states">States</h2>
""" + table(["Status", "Meaning"], [
    ["<code>pending</code>", "Registered, waiting for an admin. Its reports are refused."],
    ["<code>approved</code>", "Reports signed with its key are accepted."],
    ["<code>revoked</code>", "Its reports are refused. It stays listed until deleted."],
]) + """

<h2 id="actions">Actions</h2>
<ul>
  <li><strong>Approve</strong> a pending or revoked computer. Check first that the key fingerprint matches the one in <code>C:\\ProgramData\\DataReel\\ReportingAgent\\drra.log</code> on that computer, so you know the request came from it.</li>
  <li><strong>Revoke</strong> a computer that was lost, stolen or retired.</li>
  <li><strong>Delete registration</strong> to forget the computer. It registers again with its next report.</li>
  <li><strong>Delete report</strong> to remove its saved report file.</li>
</ul>

<h2 id="reinstall">Reinstalled computers</h2>
<p>Each hostname can only be registered with one key. When Windows is reinstalled, the computer gets a new key and its registration is refused with <code>key_mismatch</code>. Delete its old registration on the admin page, and it registers again on its next report. Approve it again, or let the enrollment token do it.</p>
""")

# ---------------------------------------------------------------- configuration
AGENT_ROWS = [
    ["delivery", "methods", "https", "One or more of <code>https</code>, <code>cifs</code>, <code>scp</code>, comma separated. Every listed method runs each cycle."],
    ["https", "host", "reports.example.com", "Report server"],
    ["https", "port", "443", "80 when <code>use_https=0</code>"],
    ["https", "upload_path", "/windows/upload/upload.php", "Web path of <code>upload.php</code>"],
    ["https", "register_path", "/windows/upload/register.php", "Web path of <code>register.php</code>"],
    ["https", "use_https", "1", "0 for plain HTTP (test servers only)"],
    ["https", "ignore_cert_revocation", "1", "Skip the CRL/OCSP check, for networks that block CRL servers (WinINet error 12057)"],
    ["https", "enrollment_token", "", "Token from the server's <code>server.ini</code>"],
    ["cifs", "share", "", "UNC path such as <code>\\\\fileserver\\reports</code>"],
    ["cifs", "user", "", "Share login for shares outside a Windows domain; save the password with <code>set-cifs-password</code>"],
    ["scp", "host / port / user", " / 22 / ", "SSH server and account"],
    ["scp", "key_file", "C:\\ProgramData\\DataReel\\ReportingAgent\\id_ed25519", "Private key with no passphrase"],
    ["scp", "remote_dir", "", "Directory on the SSH server"],
    ["scp", "known_hosts_file", "C:\\ProgramData\\DataReel\\ReportingAgent\\known_hosts", "Host keys for the server"],
    ["scp", "strict_host_key_checking", "yes", "<code>yes</code>, or <code>accept-new</code> to save the host key on the first connection"],
    ["scp", "scp_path", "%SystemRoot%\\System32\\OpenSSH\\scp.exe", "Path to <code>scp.exe</code>"],
    ["scp", "timeout_seconds", "120", "Stop a hung transfer"],
    ["agent", "report_interval_minutes", "15", "Minutes between reports, also setup <code>/INTERVAL=</code>"],
    ["agent", "report_dir", "C:\\ProgramData\\DataReel\\ReportingAgent", "Where the report is written before delivery"],
    ["agent", "log_file", "C:\\ProgramData\\DataReel\\ReportingAgent\\drra.log", "Result of the last delivery"],
    ["agent", "users_dir", "C:\\Users", "Folder listed in the USERS section"],
    ["agent", "event_lookback_days", "30", "Days of Defender alerts to include"],
    ["agent", "data_dir", "C:\\ProgramData\\DataReel\\ReportingAgent", "Agent certificate and registration state"],
]
SERVER_ROWS = [
    ["paths", "reports_dir", "/var/www/html/windows/reports", "Where accepted reports are saved"],
    ["paths", "data_dir", "/var/lib/windows_reporting_agent", "Registrations and replay protection data"],
    ["registration", "enrollment_token", "", "Shared secret the agents send when they register"],
    ["registration", "auto_approve_with_token", "1", "1 approves an agent with the right token, 0 still needs an admin"],
    ["upload", "max_upload_bytes", "2000000", "Largest report accepted"],
    ["upload", "max_clock_skew_seconds", "300", "Largest clock difference allowed between agent and server"],
    ["admin", "require_remote_user", "1", "<code>admin.php</code>, <code>status.php</code> and <code>report.php</code> only run behind web server authentication"],
    ["admin", "login", "basic", "<code>basic</code> for the web server login, <code>drsm</code> for the DataReel System Monitor dashboard login"],
    ["admin", "drsm_config", "", "With <code>login = drsm</code>: the dashboard's <code>config/drsm.php</code>, absolute or relative to the pages folder"],
    ["status", "late_hours", "24", "A report older than this is late (warning)"],
    ["status", "missing_days", "15", "A report older than this is missing (error)"],
    ["status", "patch_max_age_days", "45", "Warn when the newest Windows update is older than this"],
    ["status", "required_apps", "", "Programs every computer must have, comma separated, <code>*</code> and <code>?</code> wildcards"],
    ["status", "required_apps_skip_hosts", "", "Computers the required programs check skips, such as <code>srv-*</code>"],
    ["status", "unwanted_apps", "", "Programs no computer should have"],
    ["status", "public", "0", "1 lets the status pages run without a login. Only for a demo with sample data."],
    ["status", "reference_time", "", "Measure report ages from this epoch time, or <code>fleet</code> for the sample fleet's time"],
]
def setting_rows(rows):
    return [[r[0], "<code>%s</code>" % r[1], ("<code>%s</code>" % r[2]) if r[2].strip(" /") else "(empty)", r[3]] for r in rows]

page("configuration.html", "Configuration",
     "Every DataReel Reporting Agent setting: the agent INI file, file share and SCP delivery, and the server's server.ini.",
     [("agent", "Agent settings"), ("cifs", "File share delivery"), ("scp", "SCP delivery"), ("server", "Server settings")],
     """
<h1>Configuration</h1>
<p class="lead">The agent reads <code>drra.ini</code> next to <code>drra_svc.exe</code> before every report, so changes take effect on the next cycle without a restart. Any setting left out uses its default.</p>

<h2 id="agent">Agent settings</h2>
""" + table(["Section", "Setting", "Default", "Meaning"], setting_rows(AGENT_ROWS)) + """
<div class="callout"><p>The installer writes the <code>[https]</code> settings from its wizard pages or command line switches. Edit the file as an administrator: it is in <code>C:\\Program Files</code>.</p></div>

<h2 id="cifs">File share delivery</h2>
""" + code([("", "[delivery]"), ("", "methods=cifs"), ("", ""), ("", "[cifs]"), ("", "share=\\\\fileserver\\reports")]) + """
<p>The service runs as LocalSystem, so on a domain it connects as the computer account (<code>DOMAIN\\HOSTNAME$</code>). Give the computer accounts, or the Domain Computers group, write access to the share and folder. The report is written under a temporary name and renamed, so a reader never sees a partial file. File share and SCP deliveries do not use the certificate registration: the share permissions or the SSH key control access.</p>
<p>Without a domain (a Samba server, a NAS or a workgroup), give the agent a share login. Set <code>user=</code> in <code>[cifs]</code> and save the password from an administrator command prompt. It asks for the password without showing it and stores it encrypted with Windows DPAPI in <code>C:\\ProgramData\\DataReel\\ReportingAgent\\cifs_password.dat</code>, which only SYSTEM and Administrators can read. Each cycle the agent connects to the share with that login, copies the report and disconnects, so it works with nobody signed in.</p>
""" + code([("", "[cifs]"), ("", "share=\\\\192.168.122.1\\wra_reports"), ("", "user=wra"), ("", ""),
            ("", '"C:\\Program Files\\DataReel\\ReportingAgent\\drra_svc.exe" set-cifs-password')]) + """
<p>A share on the computer that runs the <a href="reports.html#drsm">DataReel System Monitor</a> puts every report where DRSM reads it.</p>

<h2 id="scp">SCP delivery</h2>
<p>Create a key with no passphrase for the agent, add the public key to the SSH account's <code>authorized_keys</code>, and limit the private key to SYSTEM and Administrators:</p>
""" + code([("", 'ssh-keygen -t ed25519 -N "" -f C:\\ProgramData\\DataReel\\ReportingAgent\\id_ed25519'),
            ("", "icacls C:\\ProgramData\\DataReel\\ReportingAgent\\id_ed25519 /inheritance:r /grant SYSTEM:F /grant Administrators:F"),
            ("", "ssh-keyscan reports.example.com > C:\\ProgramData\\DataReel\\ReportingAgent\\known_hosts")]) + """
""" + code([("", "[delivery]"), ("", "methods=https,scp"), ("", ""), ("", "[scp]"), ("", "host=reports.example.com"), ("", "user=winreports"), ("", "remote_dir=/var/lib/windows_reporting_agent/reports")]) + """

<h2 id="server">Server settings</h2>
<p><code>server.ini</code> is found through the <code>WRA_SERVER_INI</code> variable that <code>install_server.sh</code> sets in <code>.htaccess</code>, or at <code>/etc/windows_reporting_agent/server.ini</code>. Keep it out of the web root: it holds the enrollment token.</p>
""" + table(["Section", "Setting", "Default", "Meaning"], setting_rows(SERVER_ROWS)) + """
""")

# ---------------------------------------------------------------- reports
page("reports.html", "Reports and status page",
     "The DataReel Reporting Agent report format and the fleet status page built from the reports.",
     [("format", "Report format"), ("sections", "Sections"), ("usage", "Usage for trends"), ("status", "Fleet status page"), ("demo", "Live demo"), ("script", "Status page script"), ("drsm", "DRSM")],
     """
<h1>Reports and status page</h1>
<p class="lead">Each computer's report is a plain text file, easy to read, grep and diff. The server keeps the latest one per computer.</p>

<h2 id="format">Report format</h2>
<p>Reports are saved as <code>&lt;hostname&gt;_windows_report_agent.txt</code> in <code>reports_dir</code>. The header lines are <code>KEY: value</code> pairs, and each section starts with its name and a colon and ends with <code>END</code> and the name:</p>
""" + code([("", "DataReel Reporting Agent report from host pc-0142"),
            ("", "VERSION: 3.3"),
            ("", "HOSTNAME: pc-0142"),
            ("", "ETIME: 1792971236"),
            ("", "DATETIME: Sun Oct 25 19:33:56 2026"),
            ("", ""),
            ("", "SERVICE TAG AND BARCODE:"),
            ("", "..."),
            ("", "END SERVICE TAG AND BARCODE"),
            ("", ""),
            ("", "INSTALLED WINDOWS PATCHES:"),
            ("", "Date:  2026-10-14"),
            ("", " Patch: 2026-10 Cumulative Update for Windows 11 Version 25H2 for x64-based Systems"),
            ("", "..."),
            ("", "END INSTALLED WINDOWS PATCHES")]) + """
<p><code>ETIME</code> is the report time in seconds since 1970 (UTC), for check-in age calculations.</p>

<h2 id="sections">Sections</h2>
""" + table(["Section", "Contents"], [
    ["SERVICE TAG AND BARCODE", "Service tag (BIOS serial number, the Dell service tag on Dell computers) and asset tag from WMI"],
    ["IP ADDRESS", "IPv4 addresses of the network adapters that are up"],
    ["PERFORMANCE", "CPU count and use (sampled over 5 seconds), memory total, available and used, commit charge, processes, threads, uptime"],
    ["DISK USAGE", "Size, used, free and percent used of each local fixed drive"],
    ["NETWORK", "Per adapter that is up: link speed, bytes, packets, errors and discards received and sent, totals since the adapter came up"],
    ["USERS", "Profile folders in <code>users_dir</code>"],
    ["ALL INSTALLED PROGRAMS", "Name and version of every program in the 64-bit and 32-bit uninstall registry keys"],
    ["WINDOWS DEFENDER ALERTS", "Critical, error and warning events from the Defender event log for <code>event_lookback_days</code>"],
    ["INSTALLED WINDOWS PATCHES", "Windows Update installation history"],
    ["HOT FIXES", "Installed hotfixes from WMI (<code>Win32_QuickFixEngineering</code>)"],
]) + """

<h2 id="usage">Usage for trends</h2>
<p>PERFORMANCE, DISK USAGE and NETWORK are <code>name=value</code> lines that a monitor such as the DataReel System Monitor can graph. The names match the DRSM Linux metrics where there is one. The network counters are running totals, so take the difference between two reports for a rate. Idle virtual adapters with no traffic are left out. <code>report.php</code> shows them as a Usage panel.</p>
""" + code([("", "PERFORMANCE:"), ("", "cpu_count=4"), ("", "cpu_used_pct=7.2"), ("", "mem_total_mb=16364"), ("", "mem_available_mb=12307"),
            ("", "mem_used_pct=24.8"), ("", "commit_limit_mb=17388"), ("", "commit_used_mb=3740"), ("", "uptime_seconds=11230"), ("", "END PERFORMANCE"), ("", ""),
            ("", "DISK USAGE:"), ("", "drive=C: size_mb=510963 used_mb=108062 free_mb=402901 used_pct=21.1"), ("", "END DISK USAGE"), ("", ""),
            ("", "NETWORK:"), ("", 'adapter="Ethernet" if_index=10 speed_mbps=1000 rx_bytes=91234664 tx_bytes=37097265 rx_errors=0 tx_errors=0 ...'), ("", "END NETWORK")]) + """

<h2 id="status">Fleet status page</h2>
<p><code>status.php</code> in the upload pages folder lists every computer with its state, worst first. See it on the <a href="https://datareel.com/windows_reporting_agent/demo/">live demo</a>.</p>
""" + table(["State", "When"], [
    ['<span class="badge">Needs attention</span>', "No report for <code>missing_days</code>, an empty or bad report, or a Windows Defender detection, failed action or disabled protection"],
    ['<span class="badge">Warning</span>', "A late report, Windows updates older than <code>patch_max_age_days</code>, a missing required program or an unwanted program"],
    ['<span class="badge">OK</span>', "None of the above"],
]) + """
<p>The tiles at the top filter the list, the column headers sort it, and the search box matches computer names, users, service tags, IP addresses and problems. <code>report.php?host=&lt;name&gt;</code> shows one computer: its problems, hardware, users, Defender events, installed programs with a filter, Windows updates, hotfixes and the report file. Both pages are read only and need the admin login. Set the rules in <code>server.ini</code>:</p>
""" + code([("", "[status]"),
            ("", 'required_apps = "Microsoft 365 Apps*, Google Chrome"'),
            ("", 'required_apps_skip_hosts = "srv-*"'),
            ("", 'unwanted_apps = "uTorrent*, CCleaner*"')]) + """

<h2 id="demo">Live demo and sample fleet</h2>
<p><code>tools/gen_sample_fleet.py</code> writes reports for a made up fleet that covers every state: current, late and missing check-ins, empty and bad reports, computers behind on updates, missing and unwanted programs and Defender detections. <code>php/install_demo.sh</code> installs the status pages in a folder of their own with such a fleet and no login, which is how the <a href="https://datareel.com/windows_reporting_agent/demo/">live demo</a> runs:</p>
""" + code([("$", "php/install_demo.sh --apply /var/www/html/windows_reporting_agent/demo")]) + """

<h2 id="script">Status page script</h2>
<p><code>bash/run_windows_laptop_inventory.sh</code> is the original status page. It reads the reports and builds an HTML and a text page that list every computer by check-in state (reported in the last 24 hours, in the last 15 days, not for more than 15 days, or an empty or unreadable report) with pass and fail lists. It uses the DRSM configuration (<code>~/.drsm.sh</code> or <code>$BASEdir/etc/drsm.sh</code>) and runs from cron. Set <code>SYSLOG_DIR</code> at the top of the script to your <code>reports_dir</code>.</p>
""" + code([("c", "# crontab: update the status page every 30 minutes"),
            ("", "*/30 * * * * $HOME/windows_reporting_agent/bash/run_windows_laptop_inventory.sh > /dev/null 2>&1")]) + """

<h2 id="drsm">DRSM</h2>
<p>The <a href="https://datareel.com/info_drsm.php">DataReel System Monitor</a> shows Windows computers on its dashboard next to the Linux hosts. DRSM never connects to a Windows computer: its <code>wra</code> collector runs the Windows plugins on the monitor host against each computer's last report in <code>WRA_REPORTS_DIR</code>. The simplest way to get the reports there is the agent's <a href="configuration.html#cifs">file share delivery</a> to a share on the monitor host. Reports that arrive by HTTPS on another server have to be copied over.</p>
""" + table(["DRSM plugin", "Checks"], [
    ["<code>windows/checkin</code>", "The computer reported recently and completely, agent version"],
    ["<code>windows/defender</code>", "Defender threats, protection turned off, old definitions"],
    ["<code>windows/patches</code>", "Age of the newest Windows patch"],
    ["<code>windows/apps</code>", "Required and forbidden programs"],
    ["<code>windows/resources</code>", "CPU, memory, disk and network from the <a href=\"#usage\">usage sections</a>, for the trends page"],
]) + """
""" + code([("c", "# DRSM etc/hosts/_defaults.conf"),
            ("", "WRA_REPORTS_DIR=/path/to/reports"),
            ("", ""),
            ("c", "# DRSM etc/hosts/pc-0142.conf"),
            ("", "HOST=pc-0142"),
            ("", "PLATFORM=windows"),
            ("", "COLLECTOR=wra"),
            ("", 'PROBES=""'),
            ("", 'PLUGINS="windows/checkin windows/defender windows/patches windows/apps windows/resources"')]) + """
<p>See the DRSM documentation for the plugin settings. <code>windows/resources</code> needs the DRSM release after 1.31.</p>
""")

# ---------------------------------------------------------------- security
page("security.html", "Security",
     "How the DataReel Reporting Agent identifies computers, signs reports and protects the report server.",
     [("identity", "Agent identity"), ("register", "Registration"), ("upload", "Signed uploads"), ("computer", "On the computer"), ("server", "On the server"), ("limits", "Limits")],
     """
<h1>Security</h1>
<p class="lead">The report server only keeps reports that an approved computer signed with its own key. This page describes how, and what it does not cover.</p>

<h2 id="identity">Agent identity</h2>
<p>On its first run the agent creates a 3072-bit RSA key in the Microsoft Software Key Storage Provider as a machine key named <code>DataReelReportAgent</code>, marked not exportable, and a self-signed certificate for it with <code>CN=&lt;hostname&gt;</code>, saved as <code>agent_cert.pem</code> in <code>data_dir</code>. The key fingerprint shown in the log and on the admin page is the SHA-256 of the DER public key (SubjectPublicKeyInfo).</p>

<h2 id="register">Registration</h2>
<p>The agent posts its hostname, a timestamp, a random 128-bit nonce, its certificate and the enrollment token to <code>register.php</code>, signed over:</p>
""" + code([("", "register\\n<hostname>\\n<timestamp>\\n<nonce>\\n<SHA-256 hex of the certificate PEM>")]) + """
<p>The server checks that the certificate's CN is the hostname, that the signature verifies with the certificate's key (proof that the agent holds the key), that the timestamp is within <code>max_clock_skew_seconds</code>, and that the nonce is new. A hostname is bound to the first key that registers it: a different key is refused with <code>key_mismatch</code> until an admin deletes the registration.</p>

<h2 id="upload">Signed uploads</h2>
<p>Each report upload carries the hostname, a timestamp, a new nonce and a signature (RSA PKCS#1 v1.5 with SHA-256) over:</p>
""" + code([("", "upload\\n<hostname>\\n<timestamp>\\n<nonce>\\n<SHA-256 hex of the report>")]) + """
<p><code>upload.php</code> rejects the upload unless the host is registered and approved, the signature verifies with its registered certificate, the timestamp is in the window and the nonce has not been used. It saves the report as <code>&lt;hostname&gt;_windows_report_agent.txt</code> for the signing host, whatever file name was sent, so one computer cannot overwrite another computer's report. Replies are a <code>STATUS:</code> line the agent acts on: when a computer is revoked or deleted, the agent registers again on its next cycle.</p>

<h2 id="computer">On the computer</h2>
<ul>
  <li>The service runs as LocalSystem. The private key never leaves the key store.</li>
  <li>The installer limits <code>C:\\ProgramData\\DataReel\\ReportingAgent</code> (report, log, certificate, registration state) to SYSTEM and Administrators, so standard users cannot read the inventory or tamper with the agent state.</li>
  <li>The settings file is in <code>C:\\Program Files\\DataReel\\ReportingAgent</code>, which only administrators can change.</li>
  <li>A file share password is stored encrypted with DPAPI for the computer in <code>cifs_password.dat</code> in the same protected folder, never in the settings file.</li>
</ul>

<h2 id="server">On the server</h2>
<ul>
  <li><code>server.ini</code>, the registrations and the reports live in <code>DATA_DIR</code>, outside the web root or behind <code>Require all denied</code>.</li>
  <li>Hostnames are checked against <code>^[a-z0-9][a-z0-9_-]{0,62}$</code> before they are used in a file name.</li>
  <li><code>admin.php</code> needs a login (web server authentication, or the DRSM dashboard login with <code>login = drsm</code>) and a per-session CSRF token for every change, and escapes all output. <code>status.php</code> and <code>report.php</code> need the same login.</li>
  <li>Files are written to a temporary name and renamed into place.</li>
</ul>

<h2 id="limits">Limits</h2>
<ul>
  <li>The signature proves who sent a report and that it was not changed. Only HTTPS keeps the inventory private on the network.</li>
  <li>Anyone who is an administrator on a computer can make it send any report they like. The agent reports what Windows tells it.</li>
  <li>The enrollment token is a shared secret. Anyone who has it can register a computer with an unused name while auto-approval is on. Use admin approval, or change the token after a deployment, if that matters to you.</li>
  <li>File share and SCP deliveries are not signed: the share permissions or SSH key control access.</li>
</ul>
""")

# ---------------------------------------------------------------- building
page("building.html", "Build from source",
     "Build the DataReel Reporting Agent programs and setup.exe with Visual Studio and Inno Setup.",
     [("needs", "What you need"), ("build", "Build"), ("files", "Source files"), ("dev", "Development install")],
     """
<h1>Build from source</h1>
<p class="lead">The agent is plain C++ against the Win32 API, built with <code>nmake</code>. No libraries beyond the Windows SDK.</p>

<h2 id="needs">What you need</h2>
<ul>
  <li>Visual Studio 2022 or later (Community is fine) with the <strong>Desktop development with C++</strong> workload</li>
  <li><a href="https://jrsoftware.org/isinfo.php">Inno Setup 6</a> for the installer, in its default folder <code>C:\\Program Files (x86)\\Inno Setup 6</code></li>
</ul>

<h2 id="build">Build</h2>
<p>Open an <strong>x64 Native Tools Command Prompt for VS</strong> (or run <code>vcvars64.bat</code>) in the source folder:</p>
""" + code([("", "nmake               rem programs and the event message DLL"),
            ("", "nmake installer     rem output\\drra_setup_3.3.exe"),
            ("", "nmake clean")]) + """

<h2 id="files">Source files</h2>
""" + table(["File", "Contents"], [
    ["<code>drra_svc.cpp</code>", "The service: main loop, report builder"],
    ["<code>common.cpp</code>, <code>common.h</code>", "Settings, inventory collection, CIFS and SCP delivery, HTTPS upload"],
    ["<code>agent_auth.cpp</code>, <code>agent_auth.h</code>", "Machine key, certificate, signing, registration, multipart POST"],
    ["<code>send_test_report.cpp</code>", "Sends one report now"],
    ["<code>drra_messages.mc</code>", "Event log message DLL"],
    ["<code>drra_installer.iss</code>", "Inno Setup script"],
    ["<code>drra.ini</code>", "Default settings, with comments"],
    ["<code>php/</code>", "<code>register.php</code>, <code>upload.php</code>, <code>admin.php</code>, <code>status.php</code>, <code>report.php</code>, <code>wra_lib.php</code>, <code>wra_page.php</code>, <code>install_server.sh</code>, <code>install_demo.sh</code>, <code>server.ini.example</code>"],
    ["<code>bash/</code>", "Original fleet status page script"],
    ["<code>tools/</code>", "<code>gen_docs.py</code> for these pages, <code>gen_sample_fleet.py</code> for sample reports"],
]) + """

<h2 id="dev">Development install</h2>
<p><code>build.txt</code> lists the individual compiler commands and the registry keys and commands for installing the service by hand, without the installer. <code>send_test_report.exe</code> reads the INI file next to it, so you can test against a development server from the build folder.</p>
""")

# ---------------------------------------------------------------- troubleshooting
page("troubleshooting.html", "Troubleshooting",
     "DataReel Reporting Agent log messages, WinINet error codes and what to do about them.",
     [("log", "The log file"), ("messages", "Messages"), ("server", "Server side")],
     """
<h1>Troubleshooting</h1>
<p class="lead">Start with the agent log. It has the result of the last report for each delivery method.</p>

<h2 id="log">The log file</h2>
<p><code>C:\\ProgramData\\DataReel\\ReportingAgent\\drra.log</code>, readable by administrators. Run <code>send_test_report.exe</code> from an administrator command prompt in the install folder to get a fresh result right away.</p>

<h2 id="messages">Messages</h2>
""" + table(["Message", "What to do"], [
    ["<code>this agent is pending on the server</code>", "Approve the computer on the admin page (check the fingerprint in the log), or set the enrollment token"],
    ["<code>key_mismatch</code>", "The computer was reinstalled or its key changed. Delete its old registration on the admin page."],
    ["<code>revoked</code>", "An admin revoked the computer. Approve it again if that was a mistake."],
    ["<code>bad_timestamp</code>", "Fix the clock on the computer or the server (allowed difference: <code>max_clock_skew_seconds</code>)"],
    ["<code>bad_token</code>", "The enrollment token in the agent INI does not match <code>server.ini</code>"],
    ["<code>bad_signature</code>", "The report or request was changed on the way, or the key does not match the registration"],
    ["error code 12007", "The server name cannot be resolved"],
    ["error code 12029", "The server cannot be reached on that port"],
    ["error code 12057", "The certificate revocation check failed: keep <code>ignore_cert_revocation=1</code>"],
    ["error code 12175 or 12045", "Windows does not trust the server's TLS certificate"],
    ["<code>Unexpected answer from ... (HTTP 404)</code>", "<code>upload_path</code> or <code>register_path</code> is wrong"],
    ["<code>Could not copy report to \\\\...</code>", "The computer account, or the <code>[cifs] user</code>, cannot write to the share: share permissions, folder permissions, or SELinux on a Samba server"],
    ["<code>No share password, run: ... set-cifs-password</code>", "<code>[cifs] user=</code> is set but its password was never saved"],
    ["<code>Could not connect to \\\\... as ...</code>, error code 1326", "Wrong share user name or password: run <code>set-cifs-password</code> again"],
    ["error code 53 (network path not found)", "The computer cannot reach the file server on port 445"],
    ["error code 1219 (multiple connections)", "Another login to the same server with other credentials is open, for example a mapped drive in a desktop session"],
    ["<code>scp ... failed: Host key verification failed</code>", "Add the server to <code>known_hosts_file</code>, or use <code>accept-new</code> once"],
]) + """

<h2 id="server">Server side</h2>
<ul>
  <li><code>STATUS: error Could not save the registration</code> or <code>Reports directory is missing</code>: the web server user cannot write to <code>DATA_DIR</code>. Check the owner, ACL and, with SELinux, the <code>httpd_sys_rw_content_t</code> label (<code>ls -Z</code>).</li>
  <li><code>admin.php must be protected by web server authentication</code>: the <code>.htaccess</code> login is not active. Check <code>AllowOverride</code> for the folder.</li>
  <li>Check the Apache error log for PHP errors.</li>
</ul>
""")

# ---------------------------------------------------------------- changelog
page("changelog.html", "What's new",
     "DataReel Reporting Agent for Windows release history.",
     [("v33", "Release 3.3")],
     """
<h1>What's new</h1>
<p class="lead">Release 3.3 is the first public release of the DataReel Reporting Agent for Windows.</p>

<h2 id="v33">Release 3.3 <span class="badge new">October 2026</span></h2>
<p>Release 3.3 turns an in-house tool, written for a deployment of 120 laptops, into a package anyone can install.</p>
<ul>
  <li><strong>Signed uploads:</strong> each computer registers a certificate for a non-exportable machine key before it sends anything, and every report is signed. The server checks the signature, the approval state, the time and a one time nonce.</li>
  <li><strong>Admin page:</strong> approve, revoke and delete computers and reports, with a login and CSRF protection. It can use the DataReel System Monitor dashboard login (<code>login = drsm</code>).</li>
  <li><strong>Enrollment token:</strong> optional automatic approval for large rollouts.</li>
  <li><strong>Settings file:</strong> every setting is in <code>drra.ini</code> instead of the program code. The service reads it before each report.</li>
  <li><strong>Installer:</strong> a 64-bit setup.exe with server settings pages and command line switches for silent installs, upgrades that keep the settings, a restart-on-failure service, and an agent data folder limited to SYSTEM and Administrators.</li>
  <li><strong>Fleet status pages:</strong> <code>status.php</code> and <code>report.php</code> check every computer's last report for late check-ins, old Windows updates, Defender problems, and required and unwanted programs, with a <a href="https://datareel.com/windows_reporting_agent/demo/">live demo</a> on a sample fleet.</li>
  <li><strong>Server install script:</strong> <code>install_server.sh</code> sets up the PHP pages, <code>.htaccess</code>, admin login and data folder, with a dry run first.</li>
  <li><strong>Delivery methods:</strong> signed HTTPS uploads, a Windows file share (CIFS) with a share login for sites without a domain (password encrypted with DPAPI), and SCP, alone or together.</li>
  <li><strong>Usage for trends:</strong> PERFORMANCE (CPU, memory, commit charge, uptime), DISK USAGE (each fixed drive) and NETWORK (adapter counters) sections as <code>name=value</code> lines for DRSM trend graphs, shown on the report page.</li>
  <li><strong>Every 15 minutes:</strong> reports every 15 minutes by default, set with <code>report_interval_minutes</code> or setup <code>/INTERVAL=</code>.</li>
  <li><strong>DataReel names:</strong> service <code>DataReelReportAgent</code>, programs in <code>C:\\Program Files\\DataReel\\ReportingAgent</code>, data in <code>C:\\ProgramData\\DataReel\\ReportingAgent</code>, so nothing installed looks like part of Windows. Setup moves earlier test builds (named Windows Reporting Agent) over.</li>
</ul>
""")
print("wrote", len(ORDER), "pages to", OUT)

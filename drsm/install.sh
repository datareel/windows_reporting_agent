#!/bin/bash
# Install the DRSM (DataReel System Monitor) Windows plugins that read
# Windows Reporting Agent reports into a DRSM installation. Dry run
# unless --apply.
#
# Usage: drsm/install.sh [--apply] [-t DRSMHOME]
#   --apply  Copy the files, default is to list what would be copied
#   -t       DRSM installation, default DRSMHOME from ~/.drsm.sh or
#            ~/drsm/etc/drsm.sh, else ~/drsm
#
# Copies collectors/wra.sh, plugins/windows/, utils/wra_hosts.sh and
# etc/hosts/windows.conf_example. Host files and _defaults.conf are
# never touched. DRSM 1.32 and later already include these files.
SRCdir=$(cd "$(dirname "$0")" && pwd)
FILES="collectors/wra.sh plugins/windows etc/hosts/windows.conf_example utils/wra_hosts.sh"

apply="0"
target=""
while [ $# -gt 0 ]; do
    case "${1}" in
	--apply) apply="1" ;;
	-t) target="${2}"; shift ;;
	*) sed -n '/^# Usage:/,/^#            ~\/drsm/p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
    esac
    shift
done

if [ "${target}" == "" ]; then
    for c in "${HOME}/.drsm.sh" "${HOME}/drsm/etc/drsm.sh"; do
	if [ -f "${c}" ]; then
	    target=$(bash -c "source '${c}' > /dev/null 2>&1; echo \"\${DRSMHOME}\"")
	    break
	fi
    done
fi
target="${target:-${HOME}/drsm}"

if [ ! -f "${target}/bin/drsm_run.sh" ] || [ ! -f "${target}/lib/drsm_plugin.sh" ]; then
    echo "ERROR - ${target} is not a DRSM 1.31 or later installation, use -t"
    exit 1
fi

if [ "${apply}" == "0" ]; then echo "Dry run, nothing is copied. Run with --apply to install."; fi
for f in ${FILES}; do
    if [ "${apply}" == "1" ]; then
	mkdir -p "$(dirname "${target}/${f}")"
	cp -rfv "${SRCdir}/${f}" "$(dirname "${target}/${f}")/"
    else
	(cd "${SRCdir}" && find "${f}" -type f) | sed "s#^#Would copy to ${target}/#"
    fi
done
if [ "${apply}" == "1" ]; then
    echo ""
    echo "Installed. Next: set WRA_REPORTS_DIR in ${target}/etc/hosts/_defaults.conf,"
    echo "then run ${target}/utils/wra_hosts.sh to create the Windows host files."
fi
exit 0

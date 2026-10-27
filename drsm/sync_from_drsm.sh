#!/bin/bash
# Maintainer script: refresh this folder from a dr_system_monitor
# checkout, which holds the main copy of the Windows plugins.
#
# Usage: drsm/sync_from_drsm.sh <dr_system_monitor checkout>
if [ ! -f "${1}/collectors/wra.sh" ]; then
    echo "Usage: $0 <dr_system_monitor checkout with collectors/wra.sh>"
    exit 1
fi
DSTdir=$(cd "$(dirname "$0")" && pwd)
FILES="collectors/wra.sh plugins/windows etc/hosts/windows.conf_example utils/wra_hosts.sh"
rm -rf "${DSTdir}/plugins/windows"
(cd "${1}" && tar -cf - ${FILES}) | tar -xvf - -C "${DSTdir}"

#!/bin/bash

if [ "${BASEdir}" == "" ]; then export BASEdir="${HOME}/drsm"; fi

if [ -f ${HOME}/.drsm.sh ]; then
    CONFIG_FILE=${HOME}/.drsm.sh
else
    CONFIG_FILE=${BASEdir}/etc/drsm.sh
fi

if [ ! -f ${CONFIG_FILE} ]; then
    echo "Missing base config ${CONFIG_FILE}"
    exit 1
fi

source ${CONFIG_FILE}
source ${DRSMHOME}/bin/system_functions.sh

HOST=$(hostname -s)
PROGRAMname="$0"

RUNdir="/home/admin/windows_laptop_inventory"
HTML_REPORT_DIR="/var/www/html/sysadmin/windows_laptop_inventory"
HTML_WEB_DIR="/sysadmin/windows_laptop_inventory"
HTML_REPORT=${HTML_REPORT_DIR}/windows_laptop_inventory.html
TEXT_REPORT=${HTML_REPORT_DIR}/windows_laptop_inventory.txt
TMP_HTML_REPORT=${VARdir}/windows_laptop_inventory.html
TMP_REPORT=${VARdir}/windows_laptop_inventory.txt
SYSLOG_DIR=/var/www/html/windows/reports
SYSLOG_WEB_DIR=/windows/reports
MAX_REPORT_AGE=15

cat /dev/null > ${TMP_REPORT}
cat /dev/null > ${TMP_HTML_REPORT}
mkdir -pv ${HTML_REPORT_DIR}

cat << EOF > ${TMP_HTML_REPORT}
<!-- CSS Styling -->
<style>
ol li {
  line-height: 2; /* Creates double line spacing */
}
</style>

EOF

echo "Windows Laptop Inventory" | tee -a ${TMP_REPORT}
echo "REPORT_TIME:$(TZ="UTC" date) / $(TZ="America/New_York" date)" | tee -a ${TMP_REPORT}
echo "REPORT_ETIME:$(date +%s)" | tee -a ${TMP_REPORT}
echo "" | tee -a ${TMP_REPORT}

if [ ! -d ${SYSLOG_DIR} ]; then
    echo "ERROR:Latop report DIR not found missing ${SYSLOG_DIR}" | tee -a ${TMP_REPORT}
    exit 1
fi

echo "<b>Windows Laptop Inventory</b> @ $(TZ="UTC" date) / $(TZ="America/New_York" date)" >> ${TMP_HTML_REPORT}
echo '<ol>' >> ${TMP_HTML_REPORT}

error_status='<img src="/sysadmin/images/circle1.jpg" height="12" width="12">'
pass_status='<img src="/sysadmin/images/circle2.jpg" height="12" width="12">'
warning_status='<img src="/sysadmin/images/circle3.jpg" height="12" width="12">'

files=$(ls -1 --color=none ${SYSLOG_DIR}/*windows_report_agent.txt | sort)

TOTAL_LAPTOPS=$(ls -rtal ${SYSLOG_DIR}/*windows_report_agent.txt | wc -l)
EMPTY_REPORTS=0
BAD_REPORT_FILES=0
CHECKEDIN_24HOURS=0
CHECKEDIN_15DAYS=0
NOT_CHECKEDIN_15DAYS=0

TOTAL_LAPTOPS_FILE="${HTML_REPORT_DIR}/TOTAL_LAPTOPS_FILE.txt"
EMPTY_REPORTS_FILE="${HTML_REPORT_DIR}/EMPTY_REPORTS_FILE.txt"
BAD_REPORT_FILES_FILE="${HTML_REPORT_DIR}/BAD_REPORT_FILES_FILE.txt"
CHECKEDIN_24HOURS_FILE="${HTML_REPORT_DIR}/CHECKEDIN_24HOURS_FILE.txt"
CHECKEDIN_15DAYS_FILE="${HTML_REPORT_DIR}/CHECKEDIN_15DAYS_FILE.txt"
NOT_CHECKEDIN_15DAYS_FILE="${HTML_REPORT_DIR}/NOT_CHECKEDIN_15DAYS_FILE.txt"

cat /dev/null > ${TOTAL_LAPTOPS_FILE}
cat /dev/null > ${EMPTY_REPORTS_FILE}
cat /dev/null > ${BAD_REPORT_FILES_FILE}
cat /dev/null > ${CHECKEDIN_24HOURS_FILE}
cat /dev/null > ${CHECKEDIN_15DAYS_FILE}
cat /dev/null > ${NOT_CHECKEDIN_15DAYS_FILE}

for f in ${files}; do
    LAPTOP_NAME=$(basename $f | sed s/'_windows_report_agent.txt'//g)
    echo "Reading report for ${LAPTOP_NAME} file $f" | tee -a ${TMP_REPORT}
    echo ${LAPTOP_NAME} >> ${TOTAL_LAPTOPS_FILE}
    
    if [ ! -s ${f} ]; then
	echo "ERROR:${LAPTOP_NAME} report file is empty" | tee -a ${TMP_REPORT}
	echo "" | tee -a ${TMP_REPORT}
	echo "<li><a href=\"${SYSLOG_WEB_DIR}/$(basename ${f})\">${LAPTOP_NAME}</a> - ${error_status} report file is empty</li>" >> ${TMP_HTML_REPORT}
	let EMPTY_REPORTS=EMPTY_REPORTS+1
	echo "${LAPTOP_NAME} : has an empty report file, needs report agent installed or fixed" >> ${EMPTY_REPORTS_FILE}
	continue
    fi
    
    ETIME=$(grep "^ETIME:" ${f}) 
    if [ "$ETIME" == "" ]; then
	echo "ERROR:${LAPTOP_NAME} report file missing time stamp" | tee -a ${TMP_REPORT}
	echo "" | tee -a ${TMP_REPORT}
	echo "<li><a href=\"${SYSLOG_WEB_DIR}/$(basename ${f})\">${LAPTOP_NAME}</a> - ${error_status} report file missing time stamp</li>" >> ${TMP_HTML_REPORT}
	let BAD_REPORT_FILES=BAD_REPORT_FILES+1
	echo "${LAPTOP_NAME} : has a bad report file"  >> ${BAD_REPORT_FILES_FILE}
	continue
    fi

    REPORT_HOSTNAME=$(grep '^HOSTNAME: ' ${f} | awk -F: '{ print $2 }' | sed s/' '//g)
    REPORT_ETIME=$(grep '^ETIME: ' ${f} | awk -F: '{ print $2 }' | sed s/' '//g)
    REPORT_DATETIME=$(grep '^DATETIME: ' ${f} | sed s/'DATETIME: '//g)

    echo "REPORT_HOSTNAME = ${REPORT_HOSTNAME}" | tee -a ${TMP_REPORT}
    echo "REPORT_ETIME = ${REPORT_ETIME}" | tee -a ${TMP_REPORT}
    echo "REPORT_DATETIME = ${REPORT_DATETIME}" | tee -a ${TMP_REPORT}

    ETIME=$(date +%s)
    REPORT_HOURS=0

    let REPORT_HOURS=${ETIME}-${REPORT_ETIME}
    # 86400 seconds = 1 day
    if [ ${REPORT_HOURS} -lt 86400 ]; then
	echo "INFO:${REPORT_HOSTNAME} has reported within the last 24 hours last report ${REPORT_DATETIME}" | tee -a ${TMP_REPORT}
	echo "" | tee -a ${TMP_REPORT}
	echo "<li><a href=\"${SYSLOG_WEB_DIR}/$(basename ${f})\">${LAPTOP_NAME}</a> - ${pass_status} has reported within the last 24 hours last report ${REPORT_DATETIME}</li>" >> ${TMP_HTML_REPORT}
	let CHECKEDIN_24HOURS=CHECKEDIN_24HOURS+1
	echo "${REPORT_HOSTNAME} : has checked in within the last 24 hours, last checkin ${REPORT_DATETIME}" >> ${CHECKEDIN_24HOURS_FILE} 
	continue
    fi
    MAX_DAYS_IN_SECS=$(echo "${MAX_REPORT_AGE} * 86400" |bc )

    if [ ${REPORT_HOURS} -le ${MAX_DAYS_IN_SECS} ]; then
        echo "INFO:${REPORT_HOSTNAME} has reported within the last 15 days last report ${REPORT_DATETIME}" | tee -a ${TMP_REPORT}
	echo "" | tee -a ${TMP_REPORT}
	echo "<li><a href=\"${SYSLOG_WEB_DIR}/$(basename ${f})\">${LAPTOP_NAME}</a> - ${pass_status} has reported within the last 15 days last report ${REPORT_DATETIME}</li>" >> ${TMP_HTML_REPORT}
	let CHECKEDIN_15DAYS=CHECKEDIN_15DAYS+1
	echo "${REPORT_HOSTNAME} : has checking within the last 15 days, last checking ${REPORT_DATETIME}" >> ${CHECKEDIN_15DAYS_FILE}
        continue
    fi

    if [ ${REPORT_HOURS} -gt ${MAX_DAYS_IN_SECS} ]; then
	DAYS_OLD=$(echo "${REPORT_HOURS} / 86400" | bc)
        echo "ERROR:${REPORT_HOSTNAME} has not reported within the last 15 days last report is ${DAYS_OLD} days old ${REPORT_DATETIME}" | tee -a ${TMP_REPORT}
	echo "" | tee -a ${TMP_REPORT}
	echo "<li><a href=\"${SYSLOG_WEB_DIR}/$(basename ${f})\">${LAPTOP_NAME}</a> - ${error_status} has not reported within the last 15 days last report is ${DAYS_OLD} days old ${REPORT_DATETIME}</li>" >> ${TMP_HTML_REPORT}
	let NOT_CHECKEDIN_15DAYS=NOT_CHECKEDIN_15DAYS+1
	echo "${REPORT_HOSTNAME} : has not checking for ${DAYS_OLD} days, last checkin was on ${REPORT_DATETIME}" >> ${NOT_CHECKEDIN_15DAYS_FILE}
        continue
    fi
done

echo '</ol>' >> ${TMP_HTML_REPORT}

echo '<hr />' >> ${TMP_HTML_REPORT}
echo '<p>'  >> ${TMP_HTML_REPORT}
echo "Total number of laptops = ${TOTAL_LAPTOPS}<br />" >> ${TMP_HTML_REPORT}
echo "Laptops with empty report files = ${EMPTY_REPORTS}<br />" >> ${TMP_HTML_REPORT}
echo "Laptops with bad report files = ${BAD_REPORT_FILES}<br />" >> ${TMP_HTML_REPORT}
echo "Laptops that have not checked in with the last 15 days = ${NOT_CHECKEDIN_15DAYS}<br />" >> ${TMP_HTML_REPORT}
echo "Laptops that have checked within the last 24 hours = ${CHECKEDIN_24HOURS}<br />" >> ${TMP_HTML_REPORT}
echo "Laptops that have checked withing the last 15 days = ${CHECKEDIN_15DAYS}<br />" >> ${TMP_HTML_REPORT}
echo '</p>'  >> ${TMP_HTML_REPORT}

echo '<hr />' >> ${TMP_HTML_REPORT}
echo '<p>' >> ${TMP_HTML_REPORT}
echo "<a href =\"${HTML_WEB_DIR}/windows_laptop_pass_report.txt\">Pass report</a><br />" >> ${TMP_HTML_REPORT}
echo "<a href =\"${HTML_WEB_DIR}/windows_laptop_fail_report.txt\">Fail report</a><br />" >> ${TMP_HTML_REPORT}
echo '</p>'  >> ${TMP_HTML_REPORT}

echo "" | tee -a ${TMP_REPORT}
echo "Total number of laptops = ${TOTAL_LAPTOPS}" | tee -a ${TMP_REPORT}
echo "Laptops with empty report files = ${EMPTY_REPORTS}" | tee -a ${TMP_REPORT}
echo "Laptops with bad report files = ${BAD_REPORT_FILES}" | tee -a ${TMP_REPORT}
echo "Laptops that have not checked in with the last 15 days = ${NOT_CHECKEDIN_15DAYS}" | tee -a ${TMP_REPORT}
echo "Laptops that have checked within the last 24 hours = ${CHECKEDIN_24HOURS}" | tee -a ${TMP_REPORT}
echo "Laptops that have checked withing the last 15 days = ${CHECKEDIN_15DAYS}" | tee -a ${TMP_REPORT}
echo "" | tee -a ${TMP_REPORT}

cat /dev/null > ${HTML_REPORT_DIR}/windows_laptop_fail_report.txt
cat ${EMPTY_REPORTS_FILE} >> ${HTML_REPORT_DIR}/windows_laptop_fail_report.txt
cat ${BAD_REPORT_FILES_FILE} >> ${HTML_REPORT_DIR}/windows_laptop_fail_report.txt
cat ${NOT_CHECKEDIN_15DAYS_FILE} >> ${HTML_REPORT_DIR}/windows_laptop_fail_report.txt
cat /dev/null > ${HTML_REPORT_DIR}/windows_laptop_pass_report.txt
cat ${CHECKEDIN_24HOURS_FILE} >> ${HTML_REPORT_DIR}/windows_laptop_pass_report.txt
cat ${CHECKEDIN_15DAYS_FILE} >> ${HTML_REPORT_DIR}/windows_laptop_pass_report.txt

echo "Updating Web reports"
rsync -av --force --stats --progress ${TMP_REPORT} ${TEXT_REPORT}
rsync -av --force --stats --progress ${TMP_HTML_REPORT} ${HTML_REPORT}

echo "Windows laptop inventory check complete" | tee -a ${TMP_REPORT}

exit 0


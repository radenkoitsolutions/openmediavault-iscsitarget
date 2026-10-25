#!/usr/bin/env dash
#
# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

set -e

. /usr/share/openmediavault/scripts/helper-functions

########################################################################
# Update the configuration.
# <config>
#   <services>
#     <iscsitarget>
#       <enable>0</enable>
#       <basename>iqn.yyyy-mm.reversed.domain</basename>
#       <address>0.0.0.0</address>
#       <port>3260</port>
#       <luns>
#         <lun>
#           <uuid>xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx</uuid>
#           <name>xxx</name>
#           <sharedfolderref>xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx</sharedfolderref>
#           <filename>xxx.img</filename>
#           <size>xxx</size>
#           <writeback>0</writeback>
#           <unmap>1</unmap>
#           <comment></comment>
#         </lun>
#       </luns>
#       <targets>
#         <target>
#           <uuid>xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx</uuid>
#           <enable>1</enable>
#           <name>xxx</name>
#           <luns>uuid,uuid</luns>
#           <initiators>iqn,iqn</initiators>
#           <authentication>none|chap</authentication>
#           <userid></userid>
#           <password></password>
#           <mutualuserid></mutualuserid>
#           <mutualpassword></mutualpassword>
#           <comment></comment>
#         </target>
#       </targets>
#     </iscsitarget>
#   </services>
# </config>
########################################################################
if ! omv_config_exists "/config/services/iscsitarget"; then
	# Build the IQN naming authority from the reversed host name,
	# e.g. 'nas.example.lan' becomes 'iqn.2026-10.lan.example.nas'.
	fqdn=$(hostname -f 2>/dev/null || hostname)
	case "${fqdn}" in
		*.*) ;;
		*) fqdn="${fqdn}.local" ;;
	esac
	authority=$(echo "${fqdn}" | tr '[:upper:]' '[:lower:]' | \
		awk -F. '{ for (i = NF; i > 0; i--) printf "%s%s", $i, (i > 1 ? "." : "") }')
	basename="iqn.$(date +%Y-%m).${authority}"

	omv_config_add_node "/config/services" "iscsitarget"
	omv_config_add_key "/config/services/iscsitarget" "enable" "0"
	omv_config_add_key "/config/services/iscsitarget" "basename" "${basename}"
	omv_config_add_key "/config/services/iscsitarget" "address" "0.0.0.0"
	omv_config_add_key "/config/services/iscsitarget" "port" "3260"
	omv_config_add_node "/config/services/iscsitarget" "luns"
	omv_config_add_node "/config/services/iscsitarget" "targets"
fi

exit 0

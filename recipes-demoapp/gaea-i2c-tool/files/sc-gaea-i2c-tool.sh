#!/bin/sh

set -u

json_config="/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json"

service="xyz.gaea.i2c_tool"

i2cToolObjPath="/xyz/gaea/i2c_tool"
methodIntf="xyz.gaea.i2c_tool.methods"
debugIntf="xyz.gaea.i2c_tool.debug"
debugModeProperty="debugMode"

prefixLoopbackObjPath="/xyz/gaea/i2c_tool/"
loopbackIntf="xyz.gaea.i2c_tool.loopback"

logging() {
	level="$1"
	shift 1
	printf '\n\n%s [%s] %s\n' "$(date '+%F %T')" "$level" "$*" >&2
	if [ "$level" = "ERROR" ]; then
		exit 1
	fi
}

countdown_sec() {
	i=$1
	while [ $i -gt 0 ]; do
		printf "\rWait %d sec... " "$i"
		sleep 1
		i=$((i - 1))
	done
}

show_journallog() {
	start=$1

	# Wait 5 sec with logging to systemd journal
	countdown_sec 5

	end=$(date '+%Y-%m-%d %H:%M:%S')

	logging INFO "<cmd> journalctl --since \"${start}\" --until \"${end}\""
	journalctl --since "${start}" --until "${end}"
}

find_available_i2c_bus() {
	exclude="$1"

	for dev in /dev/i2c-*; do
		[ -e "$dev" ] || continue
		bus="${dev##*-}"

		if [ "$bus" -ne "$exclude" ]; then
			echo "$bus"
			return 0
		fi
	done

	return 1
}

verify_json_setting() {

	while read -r dev; do
		_name=$(echo "$dev" | jq -r '.name')
		_i2c_bus=$(echo "$dev" | jq -r '.i2c_bus')
		_chip_addr=$(echo "$dev" | jq -r '.chip_addr')
		_start_value=$(echo "$dev" | jq -r '.start_value')
		_end_value=$(echo "$dev" | jq -r '.end_value')

		_objPath="${prefixLoopbackObjPath}${_name}"

		logging INFO "<cmd> busctl get-property ${service} ${_objPath} ${loopbackIntf} i2cBus | awk '{print \$NF}'"
		_dbus_i2c_bus=$(dbus_get_property "${_objPath}" "${loopbackIntf}" "i2cBus" | awk '{print $NF}')
		[ "$_i2c_bus" != "$_dbus_i2c_bus" ] && return 1

		logging INFO "<cmd> busctl get-property ${service} ${_objPath} ${loopbackIntf} chipAddress | awk '{print \$NF}'"
		_dbus_chip_addr=$(dbus_get_property "${_objPath}" "${loopbackIntf}" "chipAddress" | awk '{print $NF}')
		[ "$_chip_addr" != "$_dbus_chip_addr" ] && return 1

		logging INFO "<cmd> busctl get-property ${service} ${_objPath} ${loopbackIntf} startValue | awk '{print \$NF}'"
		_dbus_start_value=$(dbus_get_property "${_objPath}" "${loopbackIntf}" "startValue" | awk '{print $NF}')
		[ "$_start_value" != "$_dbus_start_value" ] && return 1

		logging INFO "<cmd> busctl get-property ${service} ${_objPath} ${loopbackIntf} endValue | awk '{print \$NF}'"
		_dbus_end_value=$(dbus_get_property "${_objPath}" "${loopbackIntf}" "endValue" | awk '{print $NF}')
		[ "$_end_value" != "$_dbus_end_value" ] && return 1
	done <<EOF
$(jq -c '.loopback_device[]' "$json_config")
EOF

	return 0
}

dbus_set_property() {
	local _object="$1"
	local _interface="$2"
	local _property="$3"
	local _value="$4"
	_label=$(busctl get-property "${service}" "${_object}" "${_interface}" "${_property}" | awk '{print $1}')
	logging INFO "<cmd> busctl set-property ${service} ${_object} ${_interface} ${_property} ${_label} ${_value}"
	busctl set-property "${service}" "${_object}" "${_interface}" "${_property}" "${_label}" "${_value}"
	if [ $? -ne 0 ]; then
		return 1
	fi
}

dbus_get_property() {
	local object="$1"
	local interface="$2"
	local property="$3"
	busctl get-property "${service}" "${object}" "${interface}" "${property}"
}

dbus_call_method() {
	local _object="$1"
	local _interface="$2"
	local _method="$3"
	shift 3;
	local _args="$@"
	_label=$(busctl introspect "${service}" "${_object}" "${_interface}" | grep "${_method}" | awk '{print $3}')
	if [ "$_label" = '-' ]; then
		_label=""
	fi
	logging INFO "<cmd> busctl call ${service} ${_object} ${_interface} "${_method}" "${_label}" ${_args}"
	busctl call "${service}" "${_object}" "${_interface}" "${_method}" "${_label}" ${_args}
}

restore_system() {
	cp "${json_config}_temp" "${json_config}"

	logging INFO "<cmd> systemctl restart gaea-i2c-tool"
	systemctl restart gaea-i2c-tool
	countdown_sec 5

	if ! systemctl is-active --quiet gaea-i2c-tool; then
		logging ERROR "failed to restart gaea-i2c-tool service"
	else
		logging INFO "gaea-i2c-tool is restarted"
	fi

	dbus_debug_mode=$(dbus_get_property "${i2cToolObjPath}" "${debugIntf}" "${debugModeProperty}" | awk '{print $NF}')
	if [ -z "$dbus_debug_mode" ]; then
		logging ERROR "failed to get "${debugModeProperty}" property"
	fi

	if [ "$dbus_debug_mode" = "true" ]; then
		dbus_set_property "${i2cToolObjPath}" "${debugIntf}" "${debugModeProperty}" "false"
	else
		logging INFO "debug mode is already disabled"
	fi

	rm "${json_config}_temp"
}

start=$(date '+%Y-%m-%d %H:%M:%S')

# Backup json config to restore system
cp "${json_config}" "${json_config}_temp"

# CHECK SERVICE STATE
logging INFO "<cmd> systemctl status gaea-i2c-tool -l"
if ! systemctl status gaea-i2c-tool -l; then
	logging ERROR "failed to get gaea-i2c-tool status"
fi

if ! systemctl is-active --quiet gaea-i2c-tool; then
	logging ERROR "gaea-i2c-tool service is inactive"
fi

logging INFO "<cmd> busctl introspect ${service} ${i2cToolObjPath}"
if ! busctl introspect ${service} ${i2cToolObjPath}; then
	logging ERROR "failed to introspect gaea-i2c-tool d-bus"
fi

if ! verify_json_setting; then
	restore_system
	show_journallog "${start}"
	logging ERROR "Mismatch detected between JSON file and D-Bus properties"
fi

logging INFO "<cmd> busctl get-property ${service} ${i2cToolObjPath} ${debugIntf} "${debugModeProperty}" | awk '{print \$NF}'"
dbus_debug_mode=$(dbus_get_property "${i2cToolObjPath}" "${debugIntf}" "${debugModeProperty}" | awk '{print $NF}')
if [ -z "$dbus_debug_mode" ]; then
	logging ERROR "failed to get "${debugModeProperty}" property"
fi

if [ "$dbus_debug_mode" = "false" ]; then
	dbus_set_property "${i2cToolObjPath}" "${debugIntf}" "${debugModeProperty}" "true"
else
	logging INFO "debug mode is already enabled"
fi

while read -r dev; do
	name=$(echo "$dev" | jq -r '.name // empty')
	i2cBus=$(echo "$dev" | jq -r '.i2c_bus // empty')
	chipAddr=$(echo "$dev" | jq -r '.chip_addr // empty')
	startValue=$(echo "$dev" | jq -r '.start_value // empty')
	endValue=$(echo "$dev" | jq -r '.end_value // empty')

	# null/empty check
	if [ -z "$name" ] || [ -z "$i2cBus" ] || [ -z "$chipAddr" ] || \
	   [ -z "$startValue" ] || [ -z "$endValue" ]; then
		logging ERROR "json field is null or missing, $dev"
		exit 1
	fi

	logging INFO "TESTING... name = $name bus=$i2cBus addr=$chipAddr range=[$startValue:$endValue]"

	ret=$(dbus_call_method "${i2cToolObjPath}" "${methodIntf}" "read" $i2cBus $chipAddr 8 | awk '{print $2}')
	if [ "$ret" != "0" ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "fail to call read method, ret $ret"
	fi

	ret=$(dbus_call_method "${i2cToolObjPath}" "${methodIntf}" "write" $i2cBus $chipAddr 1 2 | awk '{print $2}')
	if [ "$ret" != "0" ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "fail to call write method, ret $ret"
	fi

	ret=$(dbus_call_method "${i2cToolObjPath}" "${methodIntf}" "transfer" $i2cBus $chipAddr 1 4 | awk '{print $2}')
	if [ "$ret" != "0" ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "failed to call transfer method, ret $ret"
	fi

	ret=$(dbus_call_method "${prefixLoopbackObjPath}${name}" "${loopbackIntf}" "perform" | awk '{print $2}')
	# TODO: will enable this as the target device is not ready
	#if [ "$ret" != "0" ]; then
	#	restore_system
	#	show_journallog "${start}"
	#	logging ERROR "failed to call loopback_test method, ret $ret"
	#fi

	if targetBus="$(find_available_i2c_bus "$i2cBus")"; then
		dbus_set_property "${prefixLoopbackObjPath}${name}" "${loopbackIntf}" "i2cBus" "$targetBus"
		if [ $? -ne 0 ]; then
			restore_system
			show_journallog "${start}"
			logging ERROR "failed to set i2c_bus property"
		fi
	else
		logging INFO "skipped i2c_bus testing as no available bus"
	fi

	dbus_set_property "${prefixLoopbackObjPath}${name}" "${loopbackIntf}" "chipAddress" "0xFF"
	if [ $? -ne 0 ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "failed to set chip_addr property"
	fi

	dbus_set_property "${prefixLoopbackObjPath}${name}" "${loopbackIntf}" "startValue" "0x0F"
	if [ $? -ne 0 ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "failed to set start_value property"
	fi

	dbus_set_property "${prefixLoopbackObjPath}${name}" "${loopbackIntf}" "endValue" "0xF0"
	if [ $? -ne 0 ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "failed to set end_value property"
	fi

	if ! verify_json_setting; then
		restore_system
		show_journallog "${start}"
		logging ERROR "Mismatch detected between JSON file and D-Bus properties"
	fi

	logging INFO "TESTING...PASSED"

done <<EOF
$(jq -c '.loopback_device[]' "$json_config")
EOF

restore_system
show_journallog "${start}"
logging INFO "gaea i2c tool sanity check passed"

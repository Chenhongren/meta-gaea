#!/bin/sh

set -u

service="xyz.gaea.i2c_tool"
obj_path="/xyz/gaea/i2c_tool"
debug_intf="xyz.gaea.i2c_tool.debug"
loopback_intf="xyz.gaea.i2c_tool.loopback_test"
method_intf="xyz.gaea.i2c_tool.methods"

json_config="/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json"

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
	_i2c_bus=$(jq -r '.i2c_bus' "$json_config")
	_chip_addr=$(jq -r '.chip_addr' "$json_config")
	_start_value=$(jq -r '.start_value' "$json_config")
	_end_value=$(jq -r '.end_value' "$json_config")

	logging INFO "<cmd> busctl get-property ${service} ${obj_path} ${loopback_intf} i2c_bus | awk '{print \$NF}'"
	_dbus_i2c_bus=$(dbus_get_property "${loopback_intf}" "i2c_bus" | awk '{print $NF}')
	[ "$_i2c_bus" != "$_dbus_i2c_bus" ] && return 1

	logging INFO "<cmd> busctl get-property ${service} ${obj_path} ${loopback_intf} chip_addr | awk '{print \$NF}'"
	_dbus_chip_addr=$(dbus_get_property "${loopback_intf}" "chip_addr" | awk '{print $NF}')
	[ "$_chip_addr" != "$_dbus_chip_addr" ] && return 1

	logging INFO "<cmd> busctl get-property ${service} ${obj_path} ${loopback_intf} start_value | awk '{print \$NF}'"
	_dbus_start_value=$(dbus_get_property "${loopback_intf}" "start_value" | awk '{print $NF}')
	[ "$_start_value" != "$_dbus_start_value" ] && return 1

	logging INFO "<cmd> busctl get-property ${service} ${obj_path} ${loopback_intf} end_value | awk '{print \$NF}'"
	_dbus_end_value=$(dbus_get_property "${loopback_intf}" "end_value" | awk '{print $NF}')
	[ "$_end_value" != "$_dbus_end_value" ] && return 1

	return 0
}

dbus_set_property() {
	local interface="$1"
	local property="$2"
	local value="$3"
	label=$(busctl get-property "${service}" "${obj_path}" "${interface}" "${property}" | awk '{print $1}')
	logging INFO "<cmd> busctl set-property ${service} ${obj_path} ${interface} "${property}" "${label}" ${value}"
	busctl set-property "${service}" "${obj_path}" "${interface}" "${property}" "${label}" "${value}"
	if [ $? -ne 0 ]; then
		return 1
	fi
}

dbus_get_property() {
	local interface="$1"
	local property="$2"
	busctl get-property "${service}" "${obj_path}" "${interface}" "${property}"
}

dbus_call_method() {
	local interface="$1"
	local method="$2"
	shift 2;
	local args="$@"
	label=$(busctl introspect "${service}" "${obj_path}" "${interface}" | grep "${method}" | awk '{print $3}')
	if [ "$label" = '-' ]; then
		label=""
	fi
	logging INFO "<cmd> busctl call ${service} ${obj_path} ${interface} "${method}" "${label}" ${args}"
	busctl call "${service}" "${obj_path}" "${interface}" "${method}" "${label}" ${args}
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

	dbus_debug_mode=$(dbus_get_property "${debug_intf}" "debug_mode" | awk '{print $NF}')
	if [ -z "$dbus_debug_mode" ]; then
		logging ERROR "failed to get debug_mode property"
	fi

	if [ "$dbus_debug_mode" = "true" ]; then
		dbus_set_property "${debug_intf}" "debug_mode" "false"
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

logging INFO "<cmd> busctl introspect ${service} ${obj_path}"
if ! busctl introspect ${service} ${obj_path}; then
	logging ERROR "failed to introspect gaea-i2c-tool d-bus"
fi

if ! verify_json_setting; then
	restore_system
	show_journallog "${start}"
	logging ERROR "Mismatch detected between JSON file and D-Bus properties"
fi

json_config="/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json"
i2c_bus=$(jq -r '.i2c_bus' "$json_config")
chip_addr=$(jq -r '.chip_addr' "$json_config")
start_value=$(jq -r '.start_value' "$json_config")
end_value=$(jq -r '.end_value' "$json_config")

logging INFO "<cmd> busctl get-property ${service} ${obj_path} ${debug_intf} debug_mode | awk '{print \$NF}'"
dbus_debug_mode=$(dbus_get_property "${debug_intf}" "debug_mode" | awk '{print $NF}')
if [ -z "$dbus_debug_mode" ]; then
	logging ERROR "failed to get debug_mode property"
fi

if [ "$dbus_debug_mode" = "false" ]; then
	dbus_set_property "${debug_intf}" "debug_mode" "true"
else
	logging INFO "debug mode is already enabled"
fi

ret=$(dbus_call_method "${method_intf}" "read" $i2c_bus $chip_addr 8 | awk '{print $2}')
if [ "$ret" != "0" ]; then
	restore_system
	show_journallog "${start}"
	logging ERROR "fail to call read method, ret $ret"
fi

ret=$(dbus_call_method "${method_intf}" "write" $i2c_bus $chip_addr 1 2 | awk '{print $2}')
if [ "$ret" != "0" ]; then
	restore_system
	show_journallog "${start}"
	logging ERROR "fail to call write method, ret $ret"
fi

ret=$(dbus_call_method "${method_intf}" "transfer" $i2c_bus $chip_addr 1 4 | awk '{print $2}')
if [ "$ret" != "0" ]; then
	restore_system
	show_journallog "${start}"
	logging ERROR "failed to call transfer method, ret $ret"
fi

ret=$(dbus_call_method "${loopback_intf}" "loopback_test" | awk '{print $2}')
# TODO: will enable this as the target device is not ready
#if [ "$ret" != "0" ]; then
#	restore_system
#	show_journallog "${start}"
#	logging ERROR "failed to call loopback_test method, ret $ret"
#fi

if target_bus="$(find_available_i2c_bus "$i2c_bus")"; then
	dbus_set_property "${loopback_intf}" "i2c_bus" "$target_bus"
	if [ $? -ne 0 ]; then
		restore_system
		show_journallog "${start}"
		logging ERROR "failed to set i2c_bus property"
	fi
else
	logging INFO "skipped i2c_bus testing as no available bus"
fi

dbus_set_property "${loopback_intf}" "chip_addr" "0xFF"
if [ $? -ne 0 ]; then
	restore_system
	show_journallog "${start}"
	logging ERROR "failed to set chip_addr property"
fi

dbus_set_property "${loopback_intf}" "start_value" "0x0F"
if [ $? -ne 0 ]; then
	restore_system
	show_journallog "${start}"
	logging ERROR "failed to set start_value property"
fi

dbus_set_property "${loopback_intf}" "end_value" "0xF0"
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

restore_system
show_journallog "${start}"
logging INFO "gaea i2c tool sanity check passed"

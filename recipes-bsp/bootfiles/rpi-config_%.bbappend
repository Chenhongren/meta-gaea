do_deploy:append() {
	echo "# Set usb_max_current_enable=1 to enable USB boot" >> $CONFIG
	echo "usb_max_current_enable=1" >> $CONFIG
}

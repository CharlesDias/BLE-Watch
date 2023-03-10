all: build

build:
	echo "--------------- Build the firmware ---------------"
	west build --build-dir build . --pristine --board nrf52840dk_nrf52840 -- -DNCS_TOOLCHAIN_VERSION:STRING="NONE" -DDTC_OVERLAY_FILE:STRING="nrf52840dk_nrf52840.overlay" -DSHIELD:STRING="ssd1306_128x64" -DCONF_FILE:STRING="prj.conf"

clear:
	rm -rf build
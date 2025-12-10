EXTRA_IMAGE_FEATURES += "debug-tweaks"

EXTRA_USERS_PARAMS = "\
    usermod -P rootpass root; \
"

IMAGE_INSTALL:append = " \
    iproute2 \
    iw \
    wpa-supplicant \
    libgpiod \
    libgpiod-tools \
"

SYSTEMD_AUTO_ENABLE:wpa-supplicant = "enable"
SYSTEMD_SERVICE:wpa-supplicant = "wpa_supplicant.service"
WIFI_COUNTRY_CODE = "TW"

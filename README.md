# meta-gaea

## Dependencies
TODO

## Supported Platforms
- Gaea5 (based on raspberryp pi 5 hardware)

## Setup
### Collection of layers
```script
$ mkdir -p ~/gaea
$ git clone -b styhead https://git.yoctoproject.org/poky
$ git clone -b styhead git://git.yoctoproject.org/meta-raspberrypi
$ git clone -b styhead git://git.openembedded.org/meta-openembedded
$ git clone git@github.com:Chenhongren/meta-gaea.git
```
### Build target image (gaea5 as example)
```script
platfrom=gaea5
TEMPLATECONF=/home/ren/raspberrypi5/meta-gaea/conf/templates/${platfrom}
. ../poky/oe-init-build-env build-${platfrom}
bitbake core-image-minimal
```

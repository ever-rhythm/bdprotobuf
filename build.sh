#!/bin/bash

# modify this forward your comake !
export PATH=$PATH:/home/work/qinmengyao/comake_2-1-3-2304_PD_BL/

set -e

[ -d .downloads/build-frame ] || wget -nv -P .downloads/build-frame ftp://getprod:getprod@product.scm.baidu.com:/data/prod-64/inf/odp/tools/build-frame/build-frame_1-0-9_BL/output/build-frame.tar.gz
tar xzf .downloads/build-frame/build-frame.tar.gz -C .downloads/build-frame
source .downloads/build-frame/build_phpext.sh


scm_home='../../../../'
dependency=(
    'third-64/protobuf@2.4.1.1100'
)

phpext_setup_env
php_versions='7.0.8'
phpext_release_src
phpext_release_bin
phpext_release_jam
#phpext_finish

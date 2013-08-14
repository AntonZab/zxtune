BOOST_VERSION=`sed -nr 's/.*([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' index.html`
BOOST_VERSION_MINOR=`echo ${BOOST_VERSION} | cut -d. -f2`
BOOST_COMMON_DIR=/mnt/devel/zxtune/Build/boost-${BOOST_VERSION}
TARGETS="--without-mpi --without-python --without-log --without-math"
BJAM="./bjam $* --reconfigure link=static threading=multi variant=release --layout=system target-os=linux ${TARGETS}"

cat <<EOF > tools/build/v2/user-config.jam
using gcc ;
using gcc : arm : /opt/arm-linux/bin/arm-none-linux-gnueabi-g++ ;
using gcc : armhf : /opt/armhf-linux/bin/arm-linux-gnueabihf-g++ ;
using gcc : mipsel : /opt/mipsel-linux-uclibc/usr/bin/mipsel-linux-uclibc-g++ ;

project user-config
  : requirements
    <toolset>gcc-armhf:<compileflags>-march=armv6
    <toolset>gcc-armhf:<compileflags>-mfpu=vfp
    <toolset>gcc-armhf:<compileflags>-mfloat-abi=hard
    <toolset>gcc-mipsel:<define>'WCHAR_MIN=(0)'
    <toolset>gcc-mipsel:<define>'WCHAR_MAX=((1<<8*sizeof(wchar_t))-1)'
    <toolset>gcc-mipsel:<define>'get_nprocs()=(1)'
    <toolset>gcc-mipsel:<define>BOOST_FILESYSTEM_VERSION=2
;
EOF

if [ "`uname -m`" = "i686" ]; then
  ${BJAM} --prefix=${BOOST_COMMON_DIR}-linux-i686 --includedir=${BOOST_COMMON_DIR}/include toolset=gcc address-model=32 boost.locale.icu=off install
  ${BJAM} --stagedir=${BOOST_COMMON_DIR}-linux-arm toolset=gcc-arm stage
  ${BJAM} --stagedir=${BOOST_COMMON_DIR}-linux-armhf toolset=gcc-armhf stage
  #dingux mipsel is applicable only for 1.49.0 and later due to wchar_t absence
  if [ ${BOOST_VERSION_MINOR} -le 49 ]; then
    ${BJAM} --stagedir=${BOOST_COMMON_DIR}-dingux-mipsel toolset=gcc-mipsel stage
  fi
else
  ${BJAM} --stagedir=${BOOST_COMMON_DIR}-linux-x86_64 toolset=gcc address-model=64 boost.locale.icu=off stage
fi

--------------------------------------------------------------------------------
General Instructions for building HPX-5 with APEX measurement/adaptation support
--------------------------------------------------------------------------------

To include APEX support in HPX-5, add the APEX pkgconfig path to the 
PKG_CONFIG_PATH environment variable before running ./configure. 
For example:

export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/path/to/apex/installation/lib/pkgconfig

Then, add the "--with-apex" option to the configure. For example:

./configure --enable-photon --enable-mpi --with-apex={=system,PKG}

For instructions on downloading and building APEX, please see:
https://github.com/khuck/xpress-apex

To build HPX-5 with APEX on Edison, do the following:

-----------------------------------------------------
1) load the right modules
-----------------------------------------------------

module swap PrgEnv-intel PrgEnv-gnu
module load gcc/4.9.2
module load cmake/3.0.0
module load boost
module load automake/1.14
module load autoconf
module unload cray-shmem
module unload darshan
module load papi

-----------------------------------------------------
2) build APEX
-----------------------------------------------------

APEX is built with CMake. The minimum CMake settings needed
for APEX are:

-DCMAKE_INSTALL_PREFIX=<some path to an installation location>

1. Get the code:
wget https://github.com/khuck/xpress-apex/archive/v0.1.tar.gz
tar -xvzf v0.1.tar.gz
cd xpress-apex-v0.1

2. Enter the repo directory, make a build directory:

cd xpress-apex-0.1
mkdir build
cd build

3. configure using CMake:

cmake -DCMAKE_INSTALL_PREFIX=<installation-path> ..

4. build with make:

make
make test
make doc
make install

Other CMake setting, depending on you needs can be set:
https://github.com/khuck/xpress-apex/blob/master/README.rst

-----------------------------------------------------
3) Build HPX
-----------------------------------------------------

# get HPX from gitlab
git checkout git@gitlab.crest.iu.edu:extreme/hpx.git

# set environment variables for HPX for photon support
export CRAYPE_LINK_TYPE=dynamic
export HPX_PHOTON_BACKEND=ugni

#set Pkgconfig for APEX
XPRESS_APEX=${HOME}/src/xpress-apex/install/lib/pkgconfig
if [ ${PKG_CONFIG_PATH} ] ; then
    export PKG_CONFIG_PATH=$XPRESS_APEX:${PKG_CONFIG_PATH}
else
    export PKG_CONFIG_PATH=$XPRESS_APEX
fi

# set the path to hpx-autotools, wherever they are
export PATH=/path/to/install/bin:$PATH
# bootstrap to make the configure script with APEX options
./bootstrap
# run configure
./configure --prefix=path-to-hpx-install --enable-mpi CC=cc PHOTON_CARGS="--enable-ugni" --enable-photon --with-apex
make && make install

-----------------------------------------------------
4) Build LULESH
-----------------------------------------------------

# (get hpx-apps from gitlab)
cd hpx-apps/lulesh/parcels
$ export PKG_CONFIG_PATH=/path-to-hpx-install/lib/pkgconfig:$PKG_CONFIG_PATH
$ export LD_LIBRARY_PATH=/path-to-hpx-install/lib:$LD_LIBRARY_PATH

Edit the lulesh-hpx.c

diff --git a/lulesh/parcels/lulesh-hpx.c b/lulesh/parcels/lulesh-hpx.c
index 9cd2fc5..3313a46 100644
--- a/lulesh/parcels/lulesh-hpx.c
+++ b/lulesh/parcels/lulesh-hpx.c
@@ -18,6 +18,10 @@
 #include "lulesh-hpx.h"
 #include "trace.h"

+#if defined(HAVE_APEX)
+#include "apex.h"
+#endif
+
 #ifdef DIAGNOSTICS
 uint64_t remote_comms = 0;
 uint64_t total_comms = 0;
@@ -476,6 +480,10 @@ int main(int argc, char **argv) {
     printf(" Number of domains: %d nx: %d maxcycles: %d core-major ordering: %d\n",nDoms,nx,maxcycles,core_major);
   }

+#ifdef HAVE_APEX
+  apex_setup_power_cap_throttling();
+#endif
+
   e = hpx_run(&_main, &nDoms, &nx, &maxcycles);
   hpx_finalize();
   return e;

./bootstrap
./configure CC=cc CFLAGS="-O3"
make

# Set runtime environment variables for APEX before the aprun call.

export APEX_PROFILE_OUTPUT=0
export APEX_SCREEN_OUTPUT=1
export APEX_POLICY=1
export APEX_CONCURRENCY=1
export APEX_THROTTLING=1
export APEX_ENERGY_THROTTLING=1
export APEX_THROTTLING_MAX_THREADS=48
export APEX_THROTTLING_MIN_THREADS=12
export APEX_THROTTLING_MAX_WATTS=200.0
export APEX_THROTTLING_MIN_WATTS=180.0

aprun -n 16 -N 1 -j2 -d 48 ./luleshparcels -n 343 -x 48 -i 100 --hpx-threads=48

# To see the concurrency charts (contact khuck@cs.uoregon.edu for a python script
# to aggregate them into one script):

module load gnuplot
gnuplot-persist concurrency.*.gnuplot

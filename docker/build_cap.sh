#!/usr/bin/env bash

# number of processes with default value
: ${NPROC:=2}
# append the option flag --allow-run-as-root to mpiexec
export DUMMY=/opt/dummy
mkdir -p ${DUMMY}/bin
echo '#!/usr/bin/env bash' > ${DUMMY}/bin/mpiexc
echo '/usr/bin/mpiexec --allow-run-as-root "$@"' >> ${DUMMY}/bin/mpiexec
chmod +x ${DUMMY}/bin/mpiexec
export PATH=${DUMMY}/bin:${PATH}
ln -s /usr/bin/python3.5 ${DUMMY}/bin/python
ln -s /usr/bin/clang-format-3.7 ${DUMMY}/bin/clang-format
# build the code
mkdir ${PREFIX}/build/cap
cd ${PREFIX}/build/cap
cmake \
    -G "Unix Makefiles" \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_CXX_COMPILER=mpicxx \
    -D CMAKE_CXX_FLAGS="-Wall -Wextra" \
    -D BUILD_SHARED_LIBS=ON \
    -D ENABLE_PYTHON=ON \
    -D BOOST_DIR=${BOOST_DIR} \
    -D ENABLE_DEAL_II=ON \
    -D DEAL_II_DIR=${DEAL_II_DIR} \
    -D ENABLE_COVERAGE=ON \
    -D ENABLE_FORMAT=ON \
    ${PREFIX}/source/cap
make -j${NPROC} -i
# run unit tests
export LD_LIBRARY_PATH=${BOOST_DIR}/lib:${LD_LIBRARY_PATH}
ctest -j${NPROC} -V
# check code coverage
make coverage-cpp
make coverage-python
sed -i.fixpath "s|/dummy/cap||g" lcov.info
sed -i.fixpath "s|/dummy/pycap||g" lcov.info
sed -i.fixpath "s|python/pycap|python/source|g" coverage.xml

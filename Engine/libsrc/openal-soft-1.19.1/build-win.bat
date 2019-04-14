
SET SDL2DIR="c:/Users/sonneveld/Documents/SDL2-2.0.8"



if not exist "build-Windows" mkdir  "build-Windows" 

cd "build-Windows"

cmake  -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DCMAKE_INSTALL_PREFIX:PATH=C:/Users/sonneveld/Documents/agscommunity/Engine/opt-Windows -DALSOFT_BUILD_ROUTER=ON ..
cmake --build .
cmake --build . --target install

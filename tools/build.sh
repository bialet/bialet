# Clean GUI
rm -rf out/

# Build Linux app
make clean
make
mkdir -p out/linux
cp build/bialet out/linux

# Build Windows app
make clean
CC=x86_64-w64-mingw32-gcc make
mkdir -p out/win32
cp build/bialet.exe out/win32
cp /usr/x86_64-w64-mingw32/bin/*.dll out/win32

# GUI with Electron Forge
npm install

# Build GUI Linux app
npm run package -- --platform=linux --arch=x64
cp out/linux/bialet out/bialet-desktop-linux-x64/resources/
npm run make -- --skip-package --platform=linux --arch=x64

# Build GUI Windows app
npm run package -- --platform=win32 --arch=ia32
cp out/win32/* out/bialet-desktop-win32-ia32/resources/
npm run make -- --skip-package --platform=win32 --arch=ia32

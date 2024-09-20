# Build Linux app
make clean && make
cp build/bialet /tmp/
# Build Windows app
make clean && CC=x86_64-w64-mingw32-gcc make
cp /usr/x86_64-w64-mingw32/bin/*.dll build/
# Restore original
cp /tmp/bialet build

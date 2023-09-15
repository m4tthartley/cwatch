
mkdir -p build
pushd build

warnings="-Wno-incompatible-pointer-types"
gcc ../main.c -o cwatch.exe -g $warnings -std=c99

result=$?

if [ $result -eq 0 ]; then
	echo "Build successful"
else
	echo "Build failed"
fi

popd
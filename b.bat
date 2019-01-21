set WORKSPACE=%~dp0..\edk2
set PACKAGES_PATH=%~dp0..\
set PYTHON_HOME=c:\python27

pushd %WORKSPACE%
call edksetup.bat
cd %WORKSPACE%\BaseTools\Source\C
nmake 
popd 
pushd %WORKSPACE% & build -a X64 -p EfiLuaPkg\EfiLuaPkg.dsc -b RELEASE -t VS2015x86 & popd & pause
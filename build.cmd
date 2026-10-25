@echo off
setlocal EnableExtensions
pushd "%~dp0"

set "ROOT=%~dp0"
rem MSBuild necesita SolutionDir con barra final; se usa "/" para no romper el entrecomillado.
set "SOLUTION_DIR=%ROOT:\=/%"
set "CONFIG=Release"
set "PLATFORM=x64"

where python >nul 2>&1
if errorlevel 1 (
	echo [ERROR] Python no esta en PATH. Es necesario para Metamorph-Usermode\embed.py
	goto :fail
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo [ERROR] No se encontro vswhere.exe. Instala Visual Studio Build Tools.
	goto :fail
)

set "MSBUILD="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%I"
if not defined MSBUILD (
	echo [ERROR] No se encontro MSBuild.exe.
	goto :fail
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do if not defined VSINSTALL set "VSINSTALL=%%I"

rem El proyecto pide v143 (VS2022). Si no esta, usa el toolset instalado (VS18 = v145).
set "TOOLSET="
for /d %%V in ("%VSINSTALL%\MSBuild\Microsoft\VC\*") do (
	for %%T in (v143 v145 v142) do (
		if not defined TOOLSET if exist "%%V\Platforms\x64\PlatformToolsets\%%T" set "TOOLSET=%%T"
	)
)
if defined TOOLSET (echo [i] PlatformToolset: %TOOLSET%) else (echo [i] PlatformToolset: predeterminado)

set "TOOLSET_ARG="
if defined TOOLSET set "TOOLSET_ARG=/p:PlatformToolset=%TOOLSET%"

echo [i] Compilando Metamorph-Kernel...
"%MSBUILD%" "Metamorph-Kernel\Metamorph-Kernel.vcxproj" /nologo /m /v:minimal /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:SolutionDir="%SOLUTION_DIR%" /p:SkipPackageVerification=true /p:ApiValidator_Enable=false /p:SignMode=Off
if errorlevel 1 goto :fail

echo [i] Compilando Metamorph-Usermode...
"%MSBUILD%" "Metamorph-Usermode\Metamorph-Usermode.vcxproj" /nologo /m /v:minimal /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:SolutionDir="%SOLUTION_DIR%" %TOOLSET_ARG%
if errorlevel 1 goto :fail

echo [i] Compilando Metamorph-Checker...
"%MSBUILD%" "Metamorph-Checker\Metamorph-Checker.vcxproj" /nologo /m /v:minimal /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:SolutionDir="%SOLUTION_DIR%" %TOOLSET_ARG%
if errorlevel 1 goto :fail

echo.
echo [OK] Build %CONFIG% %PLATFORM% completado:
echo   %ROOT%%PLATFORM%\%CONFIG%\Metamorph-Kernel.sys
echo   %ROOT%%PLATFORM%\%CONFIG%\Metamorph.exe
echo   %ROOT%%PLATFORM%\%CONFIG%\Metamorph-Checker.exe
popd
exit /b 0

:fail
echo.
echo [ERROR] Build fallido.
popd
exit /b 1

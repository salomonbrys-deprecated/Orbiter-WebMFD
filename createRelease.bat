@echo off

mkdir WebMFD

mkdir WebMFD\WebMFD
xcopy /E /C /Y HTML WebMFD\WebMFD

mkdir WebMFD\orbitersdk\doc
xcopy /E /C /Y doc WebMFD\orbitersdk\doc

mkdir WebMFD\Modules\Plugin
xcopy /E /C /Y ..\..\..\Modules\Plugin\WebMFD.dll WebMFD\Modules\Plugin

mkdir WebMFD\Doc
copy /Y README WebMFD\Doc\WebMFD.Readme.txt

pause

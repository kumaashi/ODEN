cl /nologo /Ox /EHsc /GS- luabind.cpp oden_util.cpp user32.lib gdi32.lib lua.lib dx12_oden.cpp /Fe:luabind_dx12.exe
cl /nologo /Ox /EHsc /GS- luabind.cpp oden_util.cpp user32.lib gdi32.lib lua.lib dx11_oden.cpp /Fe:luabind_dx11.exe

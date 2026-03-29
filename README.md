# cs2-kernel-internal

found a signed vulnerable driver that maps physical memory via ZwMapViewOfSection. wanted to see what i could do with it, ended up building an internal cheat for cs2.

## how it works

1. loads a signed kernel driver + its usermode DLL
2. finds the system CR3 from the low stub (PROCESSOR_START_BLOCK)
3. walks the EPROCESS linked list to find the target process DTB
4. manual maps a payload DLL into cs2 via physical memory writes
5. hooks DX11 Present with MinHook, renders an imgui overlay

offsets for UniqueProcessId and ActiveProcessLinks are resolved dynamically at runtime by disassembling PsGetProcessId

imgui's DX11 shaders are pre-compiled with fxc and embedded as bytecode arrays. this removes the d3dcompiler_47.dll runtime dependency which crashes in manual-mapped DLLs.

## setup

place your driver files next to `injector.exe` in the `bin/` folder:
- `driver.sys` -the kernel driver
- `driver_um.dll` -its usermode DLL (+ any dependencies it needs)

the driver must expose physical memory mapping through the usermode DLL. the injector calls two exports: a map function and an unmap function.

## build

open `cs2-kernel-internal.slnx` in visual studio. build both projects (Release x64). payload must be built with `/MT` (static CRT).

## usage

1. launch cs2
2. run `injector.exe` as admin
3. press INSERT to toggle the menu

## notes

- tested on Windows 11 25H2
- drivers are not included
- payload DLL is not in cs2's module list (manual mapped)

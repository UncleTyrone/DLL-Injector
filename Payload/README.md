# Payload

Drop the DLLs you want to inject in this folder. Subfolders are included.

`inject-launcher.bat` and `inject-service.ini` refer to files **by name relative to this folder**, not by a drive path.

```
Payload\
  payload.dll
  tools\other.dll
```

Examples:

```bat
set "DLL1=payload.dll"
set "DLL2=tools\other.dll"
```

```ini
Dll=payload.dll
Dll=tools\other.dll
```

`.dll` files in this directory are gitignored. They are not part of the source tree.

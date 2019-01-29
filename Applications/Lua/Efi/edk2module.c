#include <PiDxe.h>

#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>

#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiLib.h>
#include <math.h>

#include <lua.h>
#include <lauxlib.h>

static
int
LuaEdk2_SystemTable (
  lua_State *L
  )
{
  lua_pushinteger (L, (lua_Integer) gST);
  return 1;
}

static const struct luaL_Reg Edk2ModuleLib[] = {
  {"SystemTable", LuaEdk2_SystemTable},
  {NULL, NULL},
};

int luaopen_edk2module (lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, Edk2ModuleLib, 0);

  //luaL_newlib(L, Edk2ModuleLib);
  return 1;
}
